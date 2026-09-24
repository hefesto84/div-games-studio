/*
 * port_ide_compila.c -- Hito E4: engancha Compilar/Probar del IDE portado
 * con los ejecutables nativos divc_port y div32run_port.
 *
 * El IDE original de DOS no compilaba en proceso: guardaba el fuente, salia
 * con codigo de retorno y un DIV.BAT encadenaba DIVC (compilador) y
 * DIV32RUN (interprete). Aqui se reproduce ese flujo sin salir del IDE:
 *
 *   compilar_programa()          -> vuelca source_ptr/source_len a un .prg
 *                                   y lanza divc_port.exe como subproceso,
 *                                   capturando su salida para rellenar
 *                                   numero_error/linea_error/columna_error
 *                                   (los mismos globals que mira goto_error()).
 *   port_ide_tras_compilar_ok()  -> si el usuario pidio "Probar", lanza
 *                                   div32run_port.exe sobre el .div32 y espera
 *                                   a que cierre. El IDE NO se cierra (los
 *                                   parches byte-exactos de div.cpp:1119 y
 *                                   divhandl.cpp:224-225 sustituyen el
 *                                   "salir_del_entorno=1" por una llamada aqui).
 *
 * El buffer del editor se escribe tal cual (fwrite(buffer,1,file_lon)), que
 * es exactamente lo que hace guardar_prg() (divedit.cpp:2471), asi que el
 * compilador ve el mismo texto que se guardaria.
 */

#include "global.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <direct.h>
#include <windows.h>

/* Definiciones reales de los globals del compilador (eran stubs void* en
 * port_ide_stubs.c). goto_error() (divedit.cpp:3072) los lee como int. */
int numero_error = -1;
int linea_error = 0;
int columna_error = 0;

/* Resultado del ultimo compilar_programa() */
static char ultimo_div32[_MAX_PATH + 1];
static int  ultimo_ok = 0;

/* ------------------------------------------------------------------ */
/*  Utilidades de rutas                                                */
/* ------------------------------------------------------------------ */

static int port_existe(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void port_dir_de(const char *p, char *out, size_t outsz)
{
    size_t n;
    strncpy(out, p, outsz - 1);
    out[outsz - 1] = 0;
    n = strlen(out);
    while (n > 0) {
        char c = out[n - 1];
        if (c == '\\' || c == '/') { out[n - 1] = 0; break; }
        n--;
    }
}

static void port_dir_exe(char *out, size_t outsz)
{
    char m[MAX_PATH + 1];
    DWORD n = GetModuleFileNameA(NULL, m, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { out[0] = 0; return; }
    port_dir_de(m, out, outsz);
}

/* Carpeta raiz del repo: la que contiene system\ltlex.def (divc_port necesita
 * cwd ahi porque todas sus rutas son system\*). Candidatos: exe\..\..,
 * el cwd actual y el cwd guardado al arrancar (tipo[0].path). */
static int port_repo_root(char *out, size_t outsz)
{
    char exe[_MAX_PATH + 1];
    char cand[_MAX_PATH + 1];
    static const char *suf = "\\system\\ltlex.def";

    exe[0] = 0;
    port_dir_exe(exe, sizeof exe);

    if (exe[0]) {
        /* exe = build\Release -> la raiz es build\Release\..\.. */
        snprintf(cand, sizeof cand, "%s\\..\\..", exe);
        snprintf(out, outsz, "%s%s", cand, suf);
        if (port_existe(out)) {
            /* cand es la raiz; normalizarla a absoluta */
            if (_fullpath(out, cand, outsz) == NULL) {
                strncpy(out, cand, outsz - 1);
                out[outsz - 1] = 0;
            }
            return 1;
        }
    }

    if (getcwd(cand, (int)sizeof cand) != NULL) {
        char test[_MAX_PATH + 1];
        snprintf(test, sizeof test, "%s%s", cand, suf);
        if (port_existe(test)) { strncpy(out, cand, outsz - 1); out[outsz - 1] = 0; return 1; }
    }

    if (tipo[0].path[0]) {
        snprintf(cand, sizeof cand, "%s%s", tipo[0].path, suf);
        if (port_existe(cand)) { strncpy(out, tipo[0].path, outsz - 1); out[outsz - 1] = 0; return 1; }
    }

    if (exe[0]) { strncpy(out, exe, outsz - 1); out[outsz - 1] = 0; return 1; }
    out[0] = 0;
    return 0;
}

/* Localiza una herramienta del port (divc_port.exe / div32run_port.exe):
 * primero junto al exe del IDE (build\Release), luego en build\Release del
 * repo y por ultimo en la raiz. */
static int port_tool(const char *root, const char *name, char *out, size_t outsz)
{
    char exe[_MAX_PATH + 1];
    char p[_MAX_PATH + 1];

    exe[0] = 0;
    port_dir_exe(exe, sizeof exe);

    if (exe[0]) {
        snprintf(p, sizeof p, "%s\\%s", exe, name);
        if (port_existe(p)) { strncpy(out, p, outsz - 1); out[outsz - 1] = 0; return 1; }
    }
    if (root[0]) {
        snprintf(p, sizeof p, "%s\\build\\Release\\%s", root, name);
        if (port_existe(p)) { strncpy(out, p, outsz - 1); out[outsz - 1] = 0; return 1; }
        snprintf(p, sizeof p, "%s\\%s", root, name);
        if (port_existe(p)) { strncpy(out, p, outsz - 1); out[outsz - 1] = 0; return 1; }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Compilar                                                           */
/* ------------------------------------------------------------------ */

/* Diagnostico temporal: log junto al exe */
static FILE *port_log_open(void)
{
    char exe[_MAX_PATH + 1];
    char path[_MAX_PATH + 1];
    port_dir_exe(exe, sizeof exe);
    if (exe[0]) snprintf(path, sizeof path, "%s\\ide_compile.log", exe);
    else        snprintf(path, sizeof path, "ide_compile.log");
    return fopen(path, "ab");
}

void compilar_programa(void)
{
    struct tprg *prg;
    char root[_MAX_PATH + 1];
    char exe[_MAX_PATH + 1];
    char dir[_MAX_PATH + 1];
    char absdir[_MAX_PATH + 1];
    char base[64];
    char prgpath[_MAX_PATH + 1];
    char out[_MAX_PATH + 1];
    char line[512];
    char cmd[_MAX_PATH * 2 + 64];
    char oldcwd[_MAX_PATH + 1];
    FILE *f;
    FILE *p;
    int rc;
    int err = -1, le = 0, ce = 0;
    FILE *lg = port_log_open();
    #define LOG(...) do { if (lg) { fprintf(lg, __VA_ARGS__); fputc('\n', lg); fflush(lg); } } while (0)

    numero_error = -1;
    ultimo_ok = 0;
    ultimo_div32[0] = 0;

    LOG("=== compilar_programa ===");
    LOG("source_ptr=%p source_len=%d", source_ptr, source_len);
    if (source_ptr == NULL || source_len <= 0) { LOG("source_ptr/source_len invalido"); numero_error = 0; if (lg) fclose(lg); return; }

    prg = v.prg;
    if (prg == NULL && v_ventana >= 0 && v_ventana < max_windows) prg = ventana[v_ventana].prg;
    if (prg == NULL) prg = v_prg;
    LOG("prg=%p", prg);
    if (prg) { LOG("prg->path='%s' prg->filename='%s'", prg->path, prg->filename); }

    /* Nombre base (sin extension) */
    base[0] = 0;
    if (prg && prg->filename[0]) { strncpy(base, prg->filename, sizeof base - 1); base[sizeof base - 1] = 0; }
    else if (input[0])           { strncpy(base, input, sizeof base - 1); base[sizeof base - 1] = 0; }
    if (!base[0]) strcpy(base, "div_prog");
    { char *dot = strrchr(base, '.'); if (dot && dot != base) *dot = 0; }
    LOG("base='%s'", base);

    /* Directorio del programa (relativo al cwd, igual que guardar_prg) */
    dir[0] = 0;
    if (prg && prg->path[0]) { strncpy(dir, prg->path, sizeof dir - 1); dir[sizeof dir - 1] = 0; }
    else if (tipo[8].path[0]) { strncpy(dir, tipo[8].path, sizeof dir - 1); dir[sizeof dir - 1] = 0; }
    if (!dir[0]) getcwd(dir, (int)sizeof dir);
    LOG("dir='%s'", dir);

    absdir[0] = 0;
    if (dir[0]) {
        if (_fullpath(absdir, dir, sizeof absdir) == NULL) {
            strncpy(absdir, dir, sizeof absdir - 1);
            absdir[sizeof absdir - 1] = 0;
        }
    }
    LOG("absdir='%s'", absdir);

    if (absdir[0]) {
        snprintf(prgpath, sizeof prgpath, "%s\\%s.prg", absdir, base);
        snprintf(out, sizeof out, "%s\\%s.div32", absdir, base);
    } else {
        snprintf(prgpath, sizeof prgpath, "%s.prg", base);
        snprintf(out, sizeof out, "%s.div32", base);
    }
    LOG("prgpath='%s' out='%s'", prgpath, out);

    /* 1. Volcar el buffer del editor (identico a guardar_prg) */
    f = fopen(prgpath, "wb");
    if (f == NULL) { LOG("ERROR: no se pudo abrir %s", prgpath); numero_error = 0; if (lg) fclose(lg); return; }
    if (fwrite(source_ptr, 1, (size_t)source_len, f) != (size_t)source_len) {
        fclose(f);
        LOG("ERROR: fwrite fallo");
        numero_error = 0;
        if (lg) fclose(lg);
        return;
    }
    fclose(f);
    LOG("escrito %d bytes a %s", source_len, prgpath);

    /* 2. Localizar divc_port */
    if (!port_repo_root(root, sizeof root)) root[0] = 0;
    LOG("root='%s'", root);
    if (!port_tool(root, "divc_port.exe", exe, sizeof exe)) {
        LOG("ERROR: no se encuentra divc_port.exe");
        MessageBoxA(NULL,
            "No se encuentra divc_port.exe junto a div_ide_port.exe.",
            "DIV IDE (port)", MB_ICONERROR | MB_OK);
        numero_error = 0;
        if (lg) fclose(lg);
        return;
    }
    LOG("exe='%s'", exe);

    /* 3. Compilar con cwd = raiz del repo (system\ltlex.def, LENGUAJE.DIV).
     * Usamos CreateProcessA con redireccion a fichero (sin shell cmd.exe). */
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa;
        HANDLE hout;
        DWORD wait_rc;
        DWORD exit_code = 0;
        char tmpfile[_MAX_PATH + 1];
        char fullcmd[_MAX_PATH * 2 + 64];

        /* fichero temporal en el directorio del exe */
        snprintf(tmpfile, sizeof tmpfile, "%s\\~divc_out.tmp", absdir[0] ? absdir : ".");
        LOG("tmpfile='%s'", tmpfile);

        ZeroMemory(&sa, sizeof sa);
        sa.nLength = sizeof sa;
        sa.bInheritHandle = TRUE;
        hout = CreateFileA(tmpfile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hout == INVALID_HANDLE_VALUE) {
            LOG("ERROR: no se pudo crear %s", tmpfile);
            numero_error = 0;
            if (lg) fclose(lg);
            return;
        }

        ZeroMemory(&si, sizeof si);
        si.cb = sizeof si;
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hout;
        si.hStdError = hout;

        /* La linea de comando va literal (sin comillas de shell) */
        snprintf(fullcmd, sizeof fullcmd, "\"%s\" \"%s\" \"%s\"", exe, prgpath, out);
        LOG("fullcmd=%s", fullcmd);

        if (!CreateProcessA(NULL, fullcmd, NULL, NULL, TRUE, 0, NULL, root, &si, &pi)) {
            DWORD e = GetLastError();
            LOG("ERROR: CreateProcessA fallo (GetLastError=%lu)", e);
            CloseHandle(hout);
            numero_error = 0;
            if (lg) fclose(lg);
            return;
        }

        wait_rc = WaitForSingleObject(pi.hProcess, 30000); /* 30 s timeout */
        if (wait_rc == WAIT_TIMEOUT) {
            LOG("ERROR: timeout esperando divc_port");
            TerminateProcess(pi.hProcess, 1);
        }
        GetExitCodeProcess(pi.hProcess, &exit_code);
        LOG("exit_code=%lu", exit_code);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hout);

        /* Leer la salida del fichero temporal */
        f = fopen(tmpfile, "rb");
        if (f) {
            while (fgets(line, sizeof line, f) != NULL) {
                int e, l, c;
                LOG("out: %s", line);
                if (sscanf(line, "Error %d (linea %d, columna %d)", &e, &l, &c) == 3) {
                    err = e; le = l; ce = c;
                } else if (sscanf(line, "Error %d", &e) == 1) {
                    err = e;
                }
            }
            fclose(f);
        }
        remove(tmpfile);
        rc = (int)exit_code;
    }
    LOG("rc=%d err=%d le=%d ce=%d", rc, err, le, ce);

    /* 4. Interpretar el resultado (patron del IDE: numero_error<0 = exito) */
    if (rc == 0 && err < 0) {
        numero_error = -1;
        ultimo_ok = 1;
        strncpy(ultimo_div32, out, sizeof ultimo_div32 - 1);
        ultimo_div32[sizeof ultimo_div32 - 1] = 0;
        LOG("OK -> div32='%s'", ultimo_div32);
    } else {
        numero_error = (err >= 0) ? err : 0;
        linea_error = le;
        columna_error = ce;
        LOG("ERROR -> numero_error=%d linea=%d columna=%d", numero_error, linea_error, columna_error);
    }
    if (lg) fclose(lg);
    #undef LOG
}

/* ------------------------------------------------------------------ */
/*  Probar (sustituye al "salir_del_entorno=1" de las rutas de "Run")  */
/* ------------------------------------------------------------------ */

/* Hito E5: donde busca los recursos el juego al "Probar".
 *
 * open_file() (f.cpp) resuelve fpg\, fnt\, pcm\, map\... SIEMPRE relativos al
 * directorio de trabajo (ver docs/architecture/13-handoff.md SS5.5), asi que
 * el cwd del runner se pone en la RAIZ DE DIV -- tipo[1].path, la carpeta del
 * ejecutable del entorno -- que es justo lo que hacia el DIV original: los
 * programas viven en PRG\ y sus recursos en FPG\, FNT\, ... colgando de la
 * raiz, y DIV.BAT lanzaba el interprete desde ahi.
 *
 * Como ademas el IDE portado compila el .div32 junto al .prg (que puede estar
 * en cualquier carpeta del usuario), se pasan raices adicionales en
 * DIV_RES_PATH, que port_res_path.c reintenta cuando la busqueda normal
 * falla:
 *
 *   1. la carpeta del propio .div32   (proyecto del usuario con sus recursos)
 *   2. la raiz del repo               (arbol de desarrollo)
 *   3. <raiz del repo>\resource       (en el repo los recursos aun no estan
 *                                      "instalados" en la raiz: wmake install
 *                                      copia resource\<x>\*.* a <destino>\<x>)
 */
static void port_ide_res_path(const char *root, const char *dir_div32)
{
    char env[_MAX_PATH * 4];
    int n;

    n = snprintf(env, sizeof env, "%s", dir_div32 ? dir_div32 : "");
    if (root && root[0] && n > 0 && n < (int)sizeof env)
        n += snprintf(env + n, sizeof env - n, ";%s;%s\\resource", root, root);
    if (n <= 0 || n >= (int)sizeof env) return;

    SetEnvironmentVariableA("DIV_RES_PATH", env);
}

void port_ide_tras_compilar_ok(void)
{
    char root[_MAX_PATH + 1];
    char exe[_MAX_PATH + 1];
    char dir[_MAX_PATH + 1];
    char raiz_div[_MAX_PATH + 1];
    char cmd[_MAX_PATH * 2 + 64];
    char oldcwd[_MAX_PATH + 1];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    if (ejecutar_programa != 1 && ejecutar_programa != 3) return; /* 0=solo compilar, 2=installer */
    if (!ultimo_ok || !ultimo_div32[0]) return;

    if (!port_repo_root(root, sizeof root)) root[0] = 0;
    if (!port_tool(root, "div32run_port.exe", exe, sizeof exe)) {
        MessageBoxA(NULL,
            "No se encuentra div32run_port.exe junto a div_ide_port.exe.",
            "DIV IDE (port)", MB_ICONERROR | MB_OK);
        return;
    }

    /* Carpeta del .div32 (primera raiz de recursos) */
    strncpy(dir, ultimo_div32, sizeof dir - 1);
    dir[sizeof dir - 1] = 0;
    port_dir_de(dir, dir, sizeof dir);
    if (!dir[0]) getcwd(dir, (int)sizeof dir);

    /* Raiz de DIV = carpeta del ejecutable del entorno (tipo[1].path, fijado
     * en div.cpp:333). Si por lo que sea no esta, se cae a la carpeta del
     * .div32, que es el comportamiento previo a E5. */
    raiz_div[0] = 0;
    if (tipo[1].path[0]) {
        strncpy(raiz_div, tipo[1].path, sizeof raiz_div - 1);
        raiz_div[sizeof raiz_div - 1] = 0;
    }
    if (!raiz_div[0]) {
        strncpy(raiz_div, dir, sizeof raiz_div - 1);
        raiz_div[sizeof raiz_div - 1] = 0;
    }

    port_ide_res_path(root, dir);

    /* El runtime hace getcwd() al inicio, asi que hay que cambiar el cwd del
     * proceso padre antes de lanzar al hijo. */
    oldcwd[0] = 0;
    getcwd(oldcwd, (int)sizeof oldcwd);
    _chdir(raiz_div);

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" \"%s\"", exe, ultimo_div32);

    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, raiz_div, &si, &pi)) {
        MessageBoxA(NULL,
            "No se ha podido lanzar div32run_port.exe.",
            "DIV IDE (port)", MB_ICONERROR | MB_OK);
        if (oldcwd[0]) _chdir(oldcwd);
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    /* Restaurar cwd del IDE */
    if (oldcwd[0]) _chdir(oldcwd);
}
