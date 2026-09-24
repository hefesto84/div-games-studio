PROGRAM SCROLLDEMO;

GLOBAL
    fpg;

BEGIN
    SET_MODE(320200);
    fpg = LOAD_FPG("LEVEL1_ALL.FPG");
    DEFINE_REGION(0, 0, 0, 320, 200);
    START_SCROLL(0, fpg, 1, 0, 0, 0);

    PLAYER(42, 672);

    REPEAT
        FRAME;
    UNTIL (KEY(_esc))
END

PROCESS PLAYER(X, Y);
PRIVATE
    vx; vy; on_ground; facing; anim_timer; anim_frame;

BEGIN
    CTYPE = C_SCROLL;
    SCROLL.CAMERA = ID;
    GRAPH = 501;
    Y = Y - 16;
    vx = 0; vy = 0; on_ground = 0; facing = 1; anim_timer = 0; anim_frame = 0;

    REPEAT
        vx = 0;
        IF (KEY(_LEFT)) vx = -2; facing = -1; END
        IF (KEY(_RIGHT)) vx = 2; facing = 1; END

        vy = vy + 1;
        IF (vy > 10) vy = 10; END
        IF (on_ground)
            IF (KEY(_UP)) vy = -12; END
        END

        // colision horizontal (esquinas del lado que avanza)
        IF (vx < 0)
            IF (MAP_GET_PIXEL(fpg, 2, X - 8 + vx, Y - 16 + 2) == 1) vx = 0;
            ELSE
                IF (MAP_GET_PIXEL(fpg, 2, X - 8 + vx, Y + 16 - 2) == 1) vx = 0; END
            END
        END
        IF (vx > 0)
            IF (MAP_GET_PIXEL(fpg, 2, X + 8 + vx, Y - 16 + 2) == 1) vx = 0;
            ELSE
                IF (MAP_GET_PIXEL(fpg, 2, X + 8 + vx, Y + 16 - 2) == 1) vx = 0; END
            END
        END
        X = X + vx;

        // colision vertical (esquinas del lado que avanza)
        on_ground = 0;
        IF (vy > 0)
            IF (MAP_GET_PIXEL(fpg, 2, X - 8 + 2, Y + 16 + vy) == 1)
                vy = 0; on_ground = 1;
            ELSE
                IF (MAP_GET_PIXEL(fpg, 2, X + 8 - 2, Y + 16 + vy) == 1)
                    vy = 0; on_ground = 1;
                END
            END
        END
        IF (vy < 0)
            IF (MAP_GET_PIXEL(fpg, 2, X - 8 + 2, Y - 16 + vy) == 1) vy = 0;
            ELSE
                IF (MAP_GET_PIXEL(fpg, 2, X + 8 - 2, Y - 16 + vy) == 1) vy = 0; END
            END
        END
        Y = Y + vy;

        IF (X < 8) X = 8; END
        IF (X > 4087) X = 4087; END
        IF (Y < 16) Y = 16; END
        IF (Y > 767) Y = 767; END

        // animacion: quieto / ciclo de carrera / salto, segun estado y direccion
        // (mismos sprites en ambas direcciones; a la izquierda se usa la version
        // volteada horizontalmente que genera level_combined_fpg.py)
        IF (on_ground)
            IF (vx == 0)
                anim_timer = 0; anim_frame = 0;
                IF (facing == 1) GRAPH = 501; ELSE GRAPH = 971; END
            ELSE
                anim_timer = anim_timer + 1;
                IF (anim_timer >= 6)
                    anim_timer = 0;
                    anim_frame = anim_frame + 1;
                    IF (anim_frame >= 4) anim_frame = 0; END
                END
                IF (facing == 1)
                    IF (anim_frame == 0) GRAPH = 506; END
                    IF (anim_frame == 1) GRAPH = 507; END
                    IF (anim_frame == 2) GRAPH = 508; END
                    IF (anim_frame == 3) GRAPH = 510; END
                ELSE
                    IF (anim_frame == 0) GRAPH = 976; END
                    IF (anim_frame == 1) GRAPH = 977; END
                    IF (anim_frame == 2) GRAPH = 978; END
                    IF (anim_frame == 3) GRAPH = 980; END
                END
            END
        ELSE
            IF (facing == 1) GRAPH = 512; ELSE GRAPH = 982; END
        END

        FRAME;
    UNTIL (KEY(_esc))
END
