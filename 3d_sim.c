#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include "vector2.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define abs(x) ((x) < 0 ? -(x) : (x))
#define sign(x) ((x) == 0 ? 0 : ((x) < 0 ? -1 : 1))
#define is_wall(m) (((m) >= 65 && (m) <= 90) || (m) == '~')
#define side_wall(m, n) ((m) == '~' ? '~' : (m) + (n) * 32)
#define is_entity(m) ((m) >= 48 && (m) <= 57)
#define pyth(a, b) sqrt((a) * (a) + (b) * (b))

/*  // Alternate controls
#define move_speed 0.2
#define move_accel 0.05
#define move_drag 0.9
*/
#define move_speed 0.06
#define move_accel 1
#define move_drag 0.8
#define perspective_mode 0
#define look_speed 0.01
#define look_drag 0.9
#define step_speed 0.1
#define hitbox_size 0.3
#define step_scale 0.5
#define height_scale 0.6
#define e_height_scale 0.4
#define e_width_scale 0.3
#define x_size 195
#define y_size 50

typedef struct { Vector2 pos, vel; float angle, angle_vel; } Player;
typedef struct { float dist; char material; } Column;
typedef struct { float dist, disp; int texture; } E_Column;

static float fov_scale = 2.0;
static Column display[x_size];
static E_Column e_display[x_size];
static char map[32][32];
static char textures[10][32][32];
static Player player;
static float step_anim = 0.0;
static float step_anim_vel = 0.0;
static bool map_visible = FALSE;


int read_file (char arr[][32], char file[]) {  // Reads a file to a 2D array
    FILE *file_data;

    file_data = fopen(file, "r");
    char c;
    int c_x = 0; int c_y = 0;
    while (TRUE) {
        c = fgetc(file_data);
        if (c == ERR) break;
        if (c < 32 || c > 126) continue;

        arr[c_x][c_y] = c;
        if (++c_x == 32) {
            c_x = 0;
            c_y++;
        }
    }

    fclose(file_data);

    return 0;
}


void e_print_col (int x, E_Column col, float max_dist) {
    if (col.dist <= 0.5 || col.dist >= max_dist) return;
    if (col.disp < 0 || col.disp > 32) return;
    float step_lower = (sin(step_anim * 2.0) * 0.06 - e_height_scale * 2) / col.dist * step_scale * e_height_scale * y_size;
    float col_size = e_height_scale * (float)y_size / col.dist;
    for (int y = 0; y < y_size; y++) {
        if (abs((y_size / 2 + step_lower) - y) <= col_size * 2) {
            float mapped_y = (y - step_lower - y_size / 2) / (col_size * 4.0) * 32 + 16;  // Percentage between top & bottom of texture
            char c = textures[col.texture][(int)col.disp][(int)mapped_y];
            if (c == '~') {  // && col.dist > 8) {
                char c_tests[4];
                float alias_range = 0.1 * col.dist;
                c_tests[0] = textures[col.texture][(int)col.disp][max((int)(mapped_y - alias_range), 0)];
                c_tests[1] = textures[col.texture][(int)col.disp][min((int)(mapped_y + alias_range), 31)];
                c_tests[2] = textures[col.texture][max((int)(col.disp - alias_range), 0)][(int)mapped_y];
                c_tests[3] = textures[col.texture][min((int)(col.disp + alias_range), 31)][(int)mapped_y];
                for (int cc = 0; cc < 4; cc++) {
                    if (c_tests[cc] != '~' && c_tests[cc] != ' ') {  // && rand() % (int)(8.0 / col.dist) == 0) {
                        c = c_tests[cc];
                        break;
                    }
                }
            }
            if (c == ' ') continue;
            if (c == '~' || c == '`') c = ' ';
            // mvprintw(y, x, "%c", textures[0][min(31, max(0, (int)col.disp))][min(31, max(0, (int)mapped_y))]);
            mvprintw(y, x, "%c", c);
            // mvprintw(y, x, "?");
        }
    }
}



void print_col (int x, Column col) {  // Prints a column
    float step_lower = sin(step_anim * 2.0) * 0.06 / col.dist * step_scale * height_scale * y_size;
    float col_size = height_scale * (float)y_size / col.dist;

    for (int y = 0; y < y_size; y++) {
        if (abs((y_size / 2 + step_lower) - y) <= col_size) {
            mvprintw(y, x, "%c", col.material);
        }
    }
}


int get_normal (float x, float y) {  // Gets the normal of a ray collision - whichever axis edge is closer
    while (x > 1) x -= 1;
    while (y > 1) y -= 1;
    return min(x, 1 - x) < min(y, 1 - y);  // TRUE: x, FALSE: y
}


Vector2 portal_move (Vector2 pos, Vector2 vel, char *material) {  // Moves something a step forward as if it were moving through a portal
    char end_portal = 0;
    Vector2 portal_dir;
    for (int i = 0; i < 32; i++) {
        *material = map[(int)pos.x][(int)pos.y];

        if (end_portal == 0) {
            if (*material == '>' && vel.x > 0) {
                end_portal = '<';
                portal_dir.x = 1; portal_dir.y = 0;
            }
            else if (*material == '<' && vel.x < 0) {
                end_portal = '>';
                portal_dir.x = -1; portal_dir.y = 0;
            }
            else if (*material == 'v' && vel.y > 0) {
                end_portal = '^';
                portal_dir.x = 0; portal_dir.y = 1;
            }
            else if (*material == '^' && vel.y < 0) {
                end_portal = 'v';
                portal_dir.x = 0; portal_dir.y = -1;
            }
        }

        if (end_portal == 0 || *material == end_portal) {
            pos = add_vectors(pos, vel);
            break;
        }

        pos = add_vectors(pos, portal_dir);
        while (pos.x < 0) pos.x += 32;
        while (pos.x > 32) pos.x -= 32;
        while (pos.y < 0) pos.y += 32;
        while (pos.y > 32) pos.y -= 32;
    }

    return pos;
}


int gen_render () {  // Raycasts the screen and stores the walls in an array
    float last_dist = 0;
    char last_material = 'X';
    int normal;
    float dist;
    char material;
    float look_angle;
    int hit_entity;
    float hit_entity_dist;

    static Vector2 ray_pos, ray_vel;
    static const float ray_res = 500;

    for (float a = -x_size / 2.0; a < x_size / 2.0; a += 1) {
        float step_side = sin(step_anim) * 0.06;
        ray_pos = player.pos;
        Vector2 ray_start_off;

        switch (perspective_mode) {
            case 0:
                ray_start_off.x = 0; ray_start_off.y = 0; // Nothing
                break;
            case 1:
                ray_start_off.x = -sin(player.angle) * a / x_size / 2; ray_start_off.y = -cos(player.angle) * a / x_size / 2; // Isometric-ish
                break;
            case 2:
                ray_start_off.x = -cos(player.angle) * 2; ray_start_off.y = -sin(player.angle) * 2; // 3rd-Person
                break;
            case 3:
                ray_start_off.x = sin(player.angle) * a / x_size; ray_start_off.y = cos(player.angle) * a / x_size; // Trippy
                break;
        }
        ray_start_off.x += -sin(player.angle) * step_side; ray_start_off.y += -cos(player.angle) * step_side; // step bob

        ray_pos = portal_move(ray_pos, ray_start_off, &material);

        look_angle = player.angle + (a / x_size * fov_scale);
        // mvprintw(a + *x_size / 2, 0, "%f %f", a, look_angle);
        ray_vel.x = cos(look_angle) / ray_res;
        ray_vel.y = sin(look_angle) / ray_res;

        dist = 0;
        material = '.';
        hit_entity = 0;
        hit_entity_dist = 1;
        while (dist < 32 * 2 * ray_res) {
            // if (ray_pos.x < 0 || ray_pos.x > 32 || ray_pos.y < 0 || ray_pos.y > 32) break;
            // material = map[(int)ray_pos.x][(int)ray_pos.y];
            bool in_bounds = (ray_pos.x >= 0.1 && ray_pos.x <= 31.9 && ray_pos.y >= 0.1 && ray_pos.y <= 31.9);
            if (in_bounds) {
                ray_pos = portal_move(ray_pos, ray_vel, &material);

                if (is_entity(material)) {
                    if (hit_entity == 0) {
                        hit_entity = 1;
                        // e_display[(int)(a + x_size / 2.0)].dist = dist / ray_res;  // pyth(player.pos.x - (int)ray_pos.x, player.pos.y - (int)ray_pos.y);
                        // e_display[(int)(a + x_size / 2.0)].disp = 16;
                        e_display[(int)(a + x_size / 2.0)].texture = material - 48;
                    } else if (hit_entity == 1) {
                        float new_hed = pyth((int)ray_pos.x + 0.5 - ray_pos.x, (int)ray_pos.y + 0.5 - ray_pos.y);
                        if (new_hed < hit_entity_dist) {
                            float rel_x = ray_pos.x;
                            while (rel_x > 1) rel_x -= 1;
                            e_display[(int)(a + x_size / 2.0)].dist = dist / ray_res;

                            float ray_angle = atan2(ray_vel.y, ray_vel.x);
                            float mid_angle = atan2((int)ray_pos.y + 0.5 - ray_pos.y, (int)ray_pos.x + 0.5 - ray_pos.x);
                            float ang_diff = ray_angle - mid_angle;
                            while (ang_diff < -M_PI) ang_diff += M_PI * 2;
                            while (ang_diff > M_PI) ang_diff -= M_PI * 2;
                            e_display[(int)(a + x_size / 2.0)].disp = sign(ang_diff) * 16 * new_hed / e_width_scale / fov_scale + 16;
                            // e_display[(int)(a + x_size / 2.0)].disp = sign(rel_x - 0.5) * 16 * new_hed + 16;
                            hit_entity_dist = new_hed;
                        } else {
                            hit_entity = 2;
                        }
                    }
                }

                if (is_wall(material)) break;
            } else {
                material = '.';
                ray_pos = add_vectors(ray_pos, ray_vel);
            }

            dist += 1.0;
        }
        if (hit_entity < 2) {
            e_display[(int)(a + x_size / 2.0)].dist = 0;
        }

        normal = get_normal(ray_pos.x, ray_pos.y);
        float dist_diff = (dist - last_dist) / ray_res;
        // bool corner = dist_diff > 2.0;
        float min_dist = 1 + pow((dist + last_dist) / ray_res * 0.04, 2);
        if (last_dist != 0 && (abs(dist_diff) > min_dist || material != last_material)) {
        // && (!is_wall(material) || material != '~') && (!is_wall(last_material) || last_material != '~')) { // || normal != last_normal)) {
            // print_col((a + *x_size / 2.0), (dist_diff > 0 ? last_dist : dist) / ray_res + 0.5, '|', y_size);
            // display[(int)(a + x_size / 2.0)].dist = (dist_diff > 0 ? last_dist : dist) / ray_res + 0.5;
            display[(int)(a + x_size / 2.0)].dist = (last_dist + dist) / 2 / ray_res + 0.1;
            display[(int)(a + x_size / 2.0)].material = '|';
        } else if (is_wall(material)) {
            // mvprintw(0, (a + *x_size / 2) * 4 + 2, "%d", dist);
            // print_col((a + *x_size / 2.0), dist / ray_res + 0.1, material + normal * 32, y_size);
            display[(int)(a + x_size / 2.0)].dist = dist / ray_res + 0.1;
            display[(int)(a + x_size / 2.0)].material = side_wall(material, normal);
        } else {
            // material = '~';
            display[(int)(a + x_size / 2.0)].material = '~';
        }
        if (display[(int)(a + x_size / 2.0)].material == '~')
            display[(int)(a + x_size / 2.0)].dist = 128;

        // mvprintw(y_size + (a + *x_size / 2.0) + 1, 0, "%f > %f  %c", abs(dist_diff), min_dist, abs(dist_diff) > min_dist ? 'A' : ' ');

        // last_normal = normal;
        last_dist = dist;
        last_material = material;
    }

    return 0;
}


void *render_thread (void *vargs) {  // Renders continuously
    while (TRUE) {
        gen_render();
        usleep(16384);
    }
}


int main (int argc, char *argv[]) {
    if (argc != 2) {
        endwin();
        printf("Please input a map file\n    %s <FILE>\n", argv[0]);
        return -1;
    }

    read_file(map, argv[1]);
    for (int t = 0; t < 10; t++) {
        char fname[64];
        snprintf(fname, 64, "./items/%d.txt", t);
        read_file(textures[t], fname);
    }

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);

    player.vel.x = 0.0;
    player.vel.y = 0.0;
    player.pos.x = 1.5;
    player.pos.y = 1.5;
    player.angle = M_PI * 0.25;
    player.angle_vel = 0.0;
    char floor = ' ';

    pthread_t rendering_thread;
    pthread_create(&rendering_thread, NULL, render_thread, NULL);

    while (TRUE) {
        clear();

        switch (getch()) {
            case 'w':
                player.vel.x += cos(player.angle) * move_speed * move_accel;
                player.vel.y += sin(player.angle) * move_speed * move_accel;
                step_anim_vel = step_speed;
                break;
            case 's':
                player.vel.x += -cos(player.angle) * move_speed * move_accel;
                player.vel.y += -sin(player.angle) * move_speed * move_accel;
                step_anim_vel = step_speed;
                break;
            case 'a':
                player.vel.x += -cos(player.angle + M_PI / 2.0) * move_speed * move_accel;
                player.vel.y += -sin(player.angle + M_PI / 2.0) * move_speed * move_accel;
                step_anim_vel = step_speed;
                break;
            case 'd':
                player.vel.x += cos(player.angle + M_PI / 2.0) * move_speed * move_accel;
                player.vel.y += sin(player.angle + M_PI / 2.0) * move_speed * move_accel;
                step_anim_vel = step_speed;
                break;
            case 'j':
                player.angle_vel = -look_speed * fov_scale;
                break;
            case 'l':
                player.angle_vel = look_speed * fov_scale;
                break;
            case 'i':
                fov_scale -= 0.1;
                if (fov_scale < 0) fov_scale = 0;
                break;
            case 'k':
                fov_scale += 0.1;
                if (fov_scale > 6.3) fov_scale = 6.3;
                break;
            case 'm':
                map_visible = !map_visible;
                break;
        }

        float cur_speed = pyth(player.vel.x, player.vel.y);
        if (cur_speed > move_speed) {
            float speed_scale = move_speed / cur_speed;
            player.vel = mult_vector(player.vel, speed_scale);
        }

        step_anim += pow(step_anim_vel * 5, 4) * 2; step_anim_vel *= 0.95;


        Vector2 moving_dir, dir_pos;

        moving_dir.x = sign(player.vel.x) * hitbox_size; moving_dir.y = 0;
        dir_pos = portal_move(player.pos, moving_dir, &floor);
        if (is_wall(map[(int)dir_pos.x][(int)dir_pos.y])) player.vel.x = 0;

        moving_dir.x = 0; moving_dir.y = sign(player.vel.y) * hitbox_size;
        dir_pos = portal_move(player.pos, moving_dir, &floor);
        if (is_wall(map[(int)dir_pos.x][(int)dir_pos.y])) player.vel.y = 0;

        // if (is_wall(map[(int)(player.pos.x + sign(player.vel.x) * hitbox_size)][(int)(player.pos.y)])) player.vel.x = 0;
        // if (is_wall(map[(int)(player.pos.x)][(int)(player.pos.y + sign(player.vel.y) * hitbox_size)])) player.vel.y = 0;
        player.pos = portal_move(player.pos, player.vel, &floor); player.vel = mult_vector(player.vel, move_drag);

        // player.pos.x += player.vel.x; player.vel.x *= move_drag;
        // player.pos.y += player.vel.y; player.vel.y *= move_drag;
        player.angle += player.angle_vel; player.angle_vel *= look_drag;
        if (player.angle < 0) player.angle += M_PI * 2;
        else if (player.angle > M_PI * 2) player.angle -= M_PI * 2;

        // Moved to separate thread
        // gen_render();  // map, display, &x_size, &y_size, &fov_scale, &player);
        for (int x = 0; x < x_size; x++) {
            print_col(x, display[x]);
            e_print_col(x, e_display[x], display[x].dist);
        }

        // render_thread(&test);
        mvprintw(y_size / 2, x_size / 2 - 1, "[ ]");
        /*
        mvprintw(0, 0, " %f ", player.angle);
        mvprintw(1, 0, " %f ", player.pos.x);
        mvprintw(2, 0, " %f ", player.pos.y);
        */
        if (map_visible) {  // Draw minimap
            for (int y = 0; y < 33; y++) {
                for (int x = 0; x < 33; x++) {
                    bool out_of_range = (x < 0 || x > 31 || y < 0 || y > 31);
                    mvprintw(y, x * 2, "%c%c", !out_of_range && (is_wall(map[x][y]) || is_entity(map[x][y])) ? map[x][y] : ' ',
                                               !out_of_range && (is_wall(map[x][y]) || is_entity(map[x][y])) ? map[x][y] : ' ');
                }
            }
            mvprintw(player.pos.y, player.pos.x * 2, "%c", (player.angle >= M_PI * 1.75 || player.angle < M_PI * 0.25) ? '>' :
                                                           (player.angle >= M_PI * 0.25 && player.angle < M_PI * 0.75) ? 'v' :
                                                           (player.angle >= M_PI * 0.75 && player.angle < M_PI * 1.25) ? '<' :
                                                           (player.angle >= M_PI * 1.25 && player.angle < M_PI * 1.75) ? '^' : '?');
        }

        refresh();
        // usleep(15000);
        usleep(16384);
    }

    endwin();
}