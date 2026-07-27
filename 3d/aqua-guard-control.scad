// корпус
a_width = 26;
a_length = 66;
a_height = 13;
a_wall = 1.4;
a_rounding = 2;
a_pcb_height = 1.4;
a_pcb_z_offset = 2.2;
a_xy_error = 0.8;
// защёлки
l_width = 7;
l_height = 1.2;
// ограничители
d_width = 3;
d_height = 4;
// кнопка
b_dx = 24.5;
b_dy = 11;
b_pin_diam = 3; // диаметр штырька для нажатия на кнопку
b_esp_with_btn_height = 5.5 - a_pcb_height;
// отверстия под болтики
h_diam = 2;
h_bottom_diam = 4;
//h_top_width = 8;
//h_top_depth = 5;
//h_top_height = 4.2;
h1_dx = 12; // первые (около ethernet-порта)
h1_dy = 3;
h2_dx = 7; // вторые (около гребенки)
h2_dy = 2.2;
//h_screw_diam = 4.2;
//h_screw_height = 1.4;
//h_screw_S = 4;
//h_screw_E = 4.32;
//h_screw_z_offset = 1.6;

// usb-порт
u_height = 6.5; //4.5;
u_width = 11.5 - u_height; //5.5;
u_offset_x = 27.6;
// ethernet-порт
e_width = 15;
e_height = 12;
e_offset_z = -1;
// гребенка
p_width = a_width;
p_height = 9;
// 2.5-jack
j_dx = 21.3;
j_dy = 4.5/2;
j_diam = 9;//4.5;

$fn = 100;

translate([0, 0, a_height/2]) box(0);
translate([0, 0, a_height/2])
translate([0, 0, a_height/2]) box(1);


use <modules.scad>

module box(part) {
    difference() {
        l_points = [-20, 23];
        h_points = [-(a_length/2 - h1_dx), a_width/2 - h1_dy, a_length/2 - h2_dx, a_width/2 - h2_dy];
        pcb_top = -a_height/2 + a_pcb_z_offset + a_pcb_height;
        cut_z = pcb_top + l_height + 2;
        usb_center_z = pcb_top + 0.9 + 2.3/2; // высота esp32 и половина высоты usb-разъёма
        eth_center_z = pcb_top - a_pcb_height + e_offset_z + e_height/2;
        jack_center_z = pcb_top + j_dy;
        union() {
            // корпус
            difference() {
                minkowski() {
                    diff = 2*a_rounding - 2*a_wall;
                    cube([a_length - diff + a_xy_error, a_width - diff + a_xy_error, a_height - diff], center = true);
                    sphere(r = a_rounding);
                }
                // вырезаем внутренность
                minkowski() {
                    diff = 2*a_rounding;
                    cube([a_length - diff + a_xy_error, a_width - diff + a_xy_error, a_height - diff], center = true);
                    sphere(r = a_rounding);
                }
                // отрезаем верх/низ
                translate([0, 0, cut_z]) rotate([part == 1 ? 180 : 0, 0, 0]) 
                    cylinder(h=a_length, d=3*a_length); // translate([0, 0, a_height/2 - 1])
            }
            if (part == 0) {
                // выступы для нижних отверстий
                for (i = [0 : 2 : len(h_points)-1]) for(xx = h_points[i], yy = [-h_points[i+1], h_points[i+1]])
                    translate([xx, yy, -a_height/2]) {
                        cylinder(h=a_pcb_z_offset, d=h_bottom_diam);
                        cylinder(h=a_pcb_z_offset + a_pcb_height, d=h_diam);
                    }
            }
            if (part == 1) {
                // вырезы под защёлки
                hl = 4*l_height;
                rl = a_wall/2 + a_xy_error;
                err = 0.1;
                for(x = l_points, y = [-1, 1]) { 
                    translate([x, y*(a_width-a_wall)/2, pcb_top + l_height/2 + err]) rotate([90 - y*90, 0, 0]) latch1(l_width, l_height, a_wall, a_wall*0.8, 0.6);
                    translate([x, y*a_width/2, pcb_top + hl/2 + l_height + err]) cube([l_width, a_wall, hl], center=true);
                    translate([x, y*(a_width/2 - rl/2 + a_xy_error), pcb_top + hl + l_height + rl/2 + err]) rotate([0, -90, 0]) round_arc(rl, l_width, 1, -1);

//                    translate([x, y*(a_width/2), zl]) rotate([90 - y*90, 0, 0]) latch1(l_width, l_height, 0.2, a_wall*0.6, 0.6);
//                    translate([x, y*(a_width - a_wall + a_xy_error)/2, zl + hl/2 - l_height/2]) cube([l_width, a_wall, hl], center=true);
//                    translate([x, y*(a_width - a_wall + a_xy_error)/2, zl + hl - l_height/2 + a_wall/2]) rotate([0, -90, 0]) round_arc(a_wall, l_width, 1, -1);
                }
                // ограничители около eth-порта
                for (y = [-1, 1]) translate([-(a_length + a_xy_error - a_wall)/2, y*(e_width/2 + 2.5), jack_center_z]) {
                    cube([a_wall, d_width, d_height], center=true);
                    translate([0, 0, d_height/2 + a_wall/2]) rotate([90, 0, 0]) round_arc(a_wall, d_width);
                }
                // штырёк для кнопки нажатия на Boot
                pin_height = a_height - a_pcb_z_offset - a_pcb_height - b_esp_with_btn_height;// - b_pin_diam/2;
                translate([-a_length/2 + b_dx, a_width/2 - b_dy, a_height/2 - pin_height]) {
                    cylinder(h=pin_height + 0.1, d=b_pin_diam);
//                    sphere(d=b_pin_diam);
                }
                // выступы для верхних отверстий
//                for (i = [0 : 2 : len(h_points)-1]) for(xx = h_points[i], yy = [-h_points[i+1], h_points[i+1]]) 
//                    translate([xx, yy, a_height/2 - h_top_height/2]) {
//                        cube([h_top_width, h_top_depth, h_top_height], center = true);
//                        yc = a_width/2 + a_wall/2 - abs(yy);
//                        translate([0, sign(yy)*yc/2, 0]) cube([h_top_width, yc, h_top_height], center = true);
//                    }
            }
        }
        
        // вырез под USB
        translate([-a_length/2 + u_offset_x, a_width/2 + 2*a_wall, usb_center_z]) rotate([90, 0, 0]) hole(u_width, u_height, 2*a_wall);
        
        if (part == 0) {
            // вырезы под защёлки с небольшим запасом по ширине и высоте
            zl = pcb_top + l_height/2 + a_xy_error/2;
            for(x = l_points, y = [-1, 1]) { 
                translate([x, y*(a_width/2 - 0.1), zl]) rotate([90 - y*90, 0, 0]) latch1(l_width + a_xy_error/2, l_height + a_xy_error/2, a_wall, a_wall, 0.6);
                translate([x, y*(a_width/2 + a_wall/4), zl + l_height/2]) cube([l_width, a_wall/2, 2*l_height], center=true);
            }
            // вырезы под нижние отверстия
//            for (i = [0 : 2 : len(h_points)-1]) for(xx = h_points[i], yy = [-h_points[i+1], h_points[i+1]]) 
//                translate([xx, yy, -a_height/2 - a_wall]) {
//                    translate([0, 0, -0.1]) cylinder(h=a_height, d=h_diam);
//                    cylinder(h=h_screw_height, d1=h_screw_diam, d2=h_diam);
//                }
        }
        if (part == 1) {
            // кнопка для нажатия на Boot
            translate([-a_length/2 + b_dx, a_width/2 - b_dy, a_height/2 - a_wall/2])
                rotate([0, 0, 0]) cut_button(9, 4*a_wall, 8, 5, 1.2);

            // вырезы под верхние отверстия
//            for (i = [0 : 2 : len(h_points)-1]) for(xx = h_points[i], yy = [-h_points[i+1], h_points[i+1]])
//                translate([xx, yy, a_height/2 - h_top_height]) {
//                    translate([0, 0, -0.1]) cylinder(h=h_top_height + a_wall/2 + 0.1, d=h_diam);
//                    translate([0, 0, h_screw_z_offset + h_screw_height/2]) {
//                        for (a=[-120, 0, 120]) rotate([0, 0,  a]) cube([h_screw_S, h_screw_E/2, h_screw_height], center=true);
//                        translate([0, -sign(yy) * h_screw_S/2, 0]) cube([h_screw_S, h_screw_S, h_screw_height], center=true);
//                    }
//                    translate([0, 0, h_screw_z_offset/2]) cylinder(h=h_screw_z_offset/2, d2=h_screw_S, d1=h_diam);
//                }
        }
        // вырез под ethernet
        translate([-a_length/2, 0, eth_center_z]) cube([3*a_wall, e_width, e_height], center=true);
        // вырез под гребенку
        translate([a_length/2, 0, pcb_top + p_height/2]) cube([3*a_wall, p_width, p_height], center=true);
        // вырез под jack-и 2.5мм
        translate([a_length/2 - j_dx, 0, jack_center_z]) rotate([90, 0, 0]) cylinder(h = 3 * a_width, d = j_diam, center=true);
    }
}

%translate([-a_length/2, 0, a_pcb_z_offset - 1])
scale(0.2536) translate([-4020, 3402.5, 0]) import("D:/Devel/Projects/openscad/temp/PCB_AquaGuardControl3.stl");
