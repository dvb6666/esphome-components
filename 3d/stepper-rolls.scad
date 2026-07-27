// Параметры
SIDE = 1; // -1 для крепления слева; 1 справа; 0 без крепления
EXT_MOTOR = false; // дополнительный разъём для мотора
EXT_USBC_1 = false; // доп. USB-C снизу
EXT_USBC_2 = true; // доп. USB-C сбоку
IR_SENS_1 = false; // инфракрасный датчик снизу
IR_SENS_2 = true; // инфракрасный датчик сверху
IR_SENS_2_ANGLE = 30 * (SIDE > 0 ? 1 : -1); // угол отклонения

// Мотор
m_diam = 28;
m_height = 20;
m_holes_dist = 35; // расстояние между центрами "ушек" креплений
m_w_width = 1.4; // ширина стенок
m_holes_diam_out = 7; // внешний диаметр "ушек"
m_holes_diam_in = 3.2; // внутренний диаметр отверстий в "ушках"
m_holes_height = 8; // высота "ушек"
m_holes_screw_offset_z = 5.5; // отступ от гаек сверху
m_shift_x = 2; // сдвиг мотора по Х, чтобы сделать больше места для проводов
m_center_to_shaft = 8; // расстояние от центра мотора до центра вала
m_bottom_width = 18.2; // ширина нижнего основания

// M3 винт
h_screw_S = 5.5;
h_screw_E = 6.35;
h_screw_height = 2.8;

// Плата 34x24
b_length = 34.2;
b_width = 24.2;
b_height = 1.8; // толщина платы/текстолита (с небольшим запасом в 0.1-0.2мм)
b_z_offset = 2; // подъём для платы
b_wall_width = 1.4; // ширина стенки
b_wall_height = 10; // высота стенки
b_btn_shift_x = 12.5;
b_btn_shift_y = 2.3;
b_btn_pin_height = 3.2; // высота штырька для нажатия на кнопку
b_btn_pin_diam = 3; // диаметр штырька для нажатия на кнопку
b_latch_width = 2*b_wall_width; // ширина защёлок для платы
b_latch_height = 1.4; // высота защёлок для платы
b_latch_depth = 0.8; // глубина защёлок для платы

// Корпус
a_wall_width = 2; // ширина стенок
a_width = m_holes_dist + m_holes_diam_out + a_wall_width; // 44
a_length = b_length + a_width/2 + m_diam/2 + m_shift_x + 2*a_wall_width; // 76
a_height = m_height + 2 * a_wall_width; // ? 30
a_rounding = 2;

// Крышка
c_holes_height = 0.5; // высота углубления под болтики на крышке
c_latch_len = 2 * a_wall_width; // длина защелок крепления крышки
c_latch_height = 1; // их высота
c_latch_width = 5;
c_latch_border_dist = 4;

// горизонтальная площадка со стороны крепления к окну
a_binding_to_shaft = 32.5; // расстояние от нижней границы до центра вала
a_binding_to_top = 12; //  расстояние крепления до верхней границы корпуса
a_binding_base_length = 35.6; // длина базы в районе крепления
a_binding_base_width = 2.8; // ширина крепления
a_binding_base_height = 21.1; // высота крепления
a_binding_base_rounding = 3; // радиус скругления углов
// "ноги" крепления
a_binding_leg_length = a_binding_to_shaft - a_width/2; // длина "ноги" крепления (10.5)
a_binding_leg_width = 4; // ширина "ноги" крепления
a_binding_leg_height = 6; // высота "ноги" крепления
// два задних продолговатых отверстия
a_binding_holes_dist = 24; // расстояние по X
a_binding_holes_diam = 3.8; // диаметр отверстия
a_binding_holes_height = 4 + a_binding_holes_diam; // высота отверстий
a_binding_holes_depth = 0.8; // сдвиг по глубине отверстий; поставить "2*a_binding_base_width", чтобы отверстия стали скозными

// IR
ir_wall = 0.8; // толщина стенки
ir_height = 7.5 + 2*ir_wall; // высота
ir_width = 6.1 + 2*ir_wall; // ширина
ir_depth = 3.7 + 2*ir_wall; // глубина


$fn = 100;


// Корпус
difference() {
    a_length_no_rounding = a_length - 2*a_rounding;
    a_width_no_rounding = a_width - 2*a_rounding;
    tmp_motor_h = m_height / 3;
    z_top = a_height - a_wall_width;
    z_screw = z_top - m_holes_screw_offset_z - h_screw_height/2;

    union() {
        // корпус
        difference() {
            tmp_h = a_height - a_rounding*3/2; // 1.5 * a_height
            tmp_l = a_length - a_width/2 - a_rounding;
            translate([0, 0, a_rounding]) minkowski() {
                union() {
                    cylinder(h=tmp_h, d=a_width_no_rounding);
                    translate([tmp_l/2, 0, tmp_h/2]) cube([tmp_l, a_width_no_rounding, tmp_h], center = true);
                }
                sphere(r = a_rounding);
            }
            // отрезаем верх
            translate([0, 0, a_height]) cylinder(h=a_height, d=3*a_width);
            // вырезаем внутренность
            translate([0, 0, a_rounding + a_wall_width]) minkowski() {
                union() {
                    cylinder(h=tmp_h, d=a_width_no_rounding - 2*a_wall_width);
                    tmp_l = tmp_l - a_wall_width;
                    translate([tmp_l/2, 0, tmp_h/2]) cube([tmp_l, a_width_no_rounding - 2*a_wall_width, tmp_h], center = true);
                }
                sphere(r = a_rounding);
            }
            // выемка сверху в половину толщины стенки
            translate([0, 0, a_height - a_wall_width/2]) cylinder(h=a_wall_width, d=a_width_no_rounding + a_wall_width);
            translate([tmp_l/2, 0, a_height]) cube([tmp_l, a_width_no_rounding + a_wall_width, a_wall_width], center = true);       
        }
        
        // крепление к стене
        if (SIDE != 0) translate([m_center_to_shaft + m_shift_x, SIDE*a_binding_to_shaft, a_height - a_binding_to_top]) 
            rotate([0, 0, 90 + SIDE*90]) difference() {
            union() {
                // основание из двух параллепипедов "крестом"
                len1 = a_binding_base_length;
                translate([-len1/2, 0, 0]) cube([len1, a_binding_base_width, a_binding_base_height - a_binding_base_rounding]);
                len2 = a_binding_base_length - 2*a_binding_base_rounding;
                translate([-len2/2, 0, 0]) cube([len2, a_binding_base_width, a_binding_base_height]);
                // и цилиндров в верхних углах для скругления
                for(x = [-1, 1])
                    translate([x * (a_binding_base_length/2 - a_binding_base_rounding), 0, a_binding_base_height - a_binding_base_rounding]) 
                        rotate([0, 90,  90]) cylinder(h=a_binding_base_width, r=a_binding_base_rounding);
                // нога
                leg_len = a_binding_leg_length + a_wall_width;
                for(x = [-1, 1]) translate([x * a_binding_holes_dist/2, leg_len/2, 0]) {
                    translate([0, 0, a_binding_leg_height/2]) cube([a_binding_leg_width, leg_len, a_binding_leg_height], center = true);
                    translate([0, 0, -a_binding_leg_length/2]) rotate([0,90,0]) round_arc(a_binding_leg_length, a_binding_leg_width, 1, -1);
                }
            }
            // два продолговатых отверстия сзади
            for (x = [-1, 1]) translate([x * a_binding_holes_dist/2, -0.1, a_binding_base_height/2]) 
                rotate([0, 90,  90]) hole(a_binding_holes_height - a_binding_holes_diam, a_binding_holes_diam, a_binding_holes_depth + 0.1);
        }
        
        // крепление под мотор
        translate([m_shift_x, 0, a_wall_width]) {
            hole_width = m_holes_diam_out + 2*a_wall_width;
            hole_depth = (a_width - m_diam) / 2;
            // цилиндр под крепление мотора
            cylinder(h=tmp_motor_h, d=m_diam + 2*m_w_width);
            // "перекладина" для ушек
            translate([0, 0, a_height - a_wall_width*3/2 - m_holes_height/2]) cube([hole_width, a_width - a_wall_width + 1, m_holes_height], center = true);
            // арки под "ушками"
            for(y = [-1, 1]) translate([0, y*(a_width_no_rounding - hole_depth)/2, z_top - m_holes_height - hole_depth/2 - a_wall_width/2]) 
                rotate([0,90,0]) round_arc(hole_depth, hole_width, 1, -y);
        }
        // крепление под плату
        latch_z_offset = b_z_offset + b_height + b_latch_height/2;
        translate([a_length - a_width/2 - a_wall_width, 0, a_wall_width]) {
            for(y = [-1, 1]) {
                xx = b_length + b_wall_width;
                yy = y * (b_width + b_wall_width)/2;
                translate([-xx/2, yy, b_wall_height/2]) cube([xx, b_wall_width, b_wall_height], center=true);
                translate([-xx/2, yy - y*b_wall_width, b_z_offset/2]) cube([xx, b_wall_width, b_z_offset], center=true);
                // сдвинутые к центру защёлки со скатами
                translate([-xx + b_wall_width/2, yy - y*1.5*b_latch_width, b_wall_height/2]) cube([b_wall_width, b_latch_width, b_wall_height], center=true);
                translate([-xx + b_wall_width*3/2, yy - y*1.5*b_latch_width, b_wall_width/2]) rotate([90, 0, 0]) round_arc(b_wall_width, b_latch_width);
                translate([-xx - b_wall_width/2, yy - y*1.5*b_latch_width, b_wall_width/2]) rotate([90, 0, 0]) round_arc(b_wall_width, b_latch_width, -1);
                translate([-xx + b_wall_width - 0.2, yy - y*1.5*b_latch_width, latch_z_offset]) 
                    rotate([0, 0, -90]) latch1(b_latch_width, b_latch_height, 0.2, b_latch_depth, b_latch_height/2);
                translate([0.2, yy - y*1.5*b_latch_width, latch_z_offset]) 
                    rotate([0, 0, 90]) latch1(b_latch_width, b_latch_height, 0.2, b_latch_depth, b_latch_height/2);
//                // по 3 защёлки с каждой стороны
//                for(x = [-1, 0, 1]) 
//                    translate([-b_length/2 + x*b_length/3, yy - y*b_wall_width/4, b_z_offset + b_height]) rotate([0, 90, 0]) cylinder(h=4, d=1, center=true);
            }
        }
        // крепление под второй (верхний) IR
        if (IR_SENS_2) rotate([0, 0, IR_SENS_2_ANGLE]) translate([-a_width/2 + ir_wall, 0, a_height - ir_height/2 - a_wall_width/2]) {
            cube([ir_depth, ir_width, ir_height], center=true);
//            translate([0, 0, ir_height/2]) cube([ir_depth - 2*ir_wall, ir_width, a_wall_width/2], center=true);
        }
    }
    
    // вырез под крепление второго (верхнего) IR
    if (IR_SENS_2) rotate([0, 0, IR_SENS_2_ANGLE]) translate([-a_width/2 + ir_wall, 0, a_height - ir_height/2 - a_wall_width/2 + ir_wall]) {
        cube([ir_depth - 2*ir_wall, ir_width - 2*ir_wall, ir_height + ir_wall], center=true);
        cube([ir_depth+1, ir_width - 3*ir_wall, ir_height], center=true);
        translate([ir_depth/2 - ir_wall, 0, -ir_height/2]) cube([ir_depth/2, ir_width - 3*ir_wall, ir_height], center=true);
        translate([0, 0, ir_height/2 + a_wall_width/4 - ir_wall + 0.5]) cube([2*ir_wall, ir_width, a_wall_width/2 + 1], center=true);
    }            

    // вырез для USB-C снизу
    translate([a_length - a_width/2 - a_wall_width - 1, 0, a_wall_width + b_z_offset + 15]) rotate([90, 0, 90]) hole(5.5, 4.5, 2*a_wall_width);
    // вырез для дополнительного USB-C снизу
    if (EXT_USBC_1) {
        translate([a_length - a_width/2 - a_wall_width - 1, 0, a_wall_width + b_z_offset + 4.3]) rotate([90, 0, 90]) hole(5.5, 4.5, 2*a_wall_width);
    } else {
        // вырез для дополнительного USB-C сбоку
        if (EXT_USBC_2) translate([a_length - a_width/2 - 8.6, 0, -1]) rotate([0, 0, 90]) hole(5.5, 4.5, 2*a_wall_width);
        // вырез для IR-сенсора
        if (IR_SENS_1) translate([a_length - a_width/2 - a_wall_width - 1, 0, a_wall_width + b_z_offset + 8]) cube([20, 7, 10], center=true);
    }
    // доп.мотор
    if (EXT_MOTOR) translate([a_length - a_width/2 - 31.5, 0, a_wall_width]) cube([7.5, 18, 3*a_wall_width], center = true);
    
    // вырезы под крепления крышки
    y_offset = a_width_no_rounding/2 - c_latch_border_dist - c_latch_width/2;
    la_height = c_latch_height + 0.2; // высота выреза с небольшим запасом
    for (y = [-1, 1]) translate([a_length - a_width/2 - a_wall_width - 1, y * y_offset, z_top + la_height/2]) {
        rotate([90, 0, 90]) hole(c_latch_width - la_height +0.2, la_height, 2*a_wall_width);
        translate([c_latch_len/2, 0, la_height/4]) cube([c_latch_len, c_latch_width + 0.2, la_height/2], center=true);
    }
        
    // вырезы под мотор
    translate([m_shift_x, 0, a_wall_width]) {
        // вырез под сам мотор
        cylinder(h=a_height, d=m_diam);
        translate([-m_diam/2, 0, tmp_motor_h/2+1]) cube([m_diam/4, m_bottom_width, tmp_motor_h+2], center=true);
        // вырезы под ушки и гайки
        translate([0, 0, z_top - a_wall_width/2]) cube([m_holes_diam_out, m_diam + m_holes_diam_out, 2], center = true);
        translate([0, 0, z_screw]) cube([h_screw_S, m_holes_dist, h_screw_height], center = true);
        for (y = [-1, 1]) {
            y_hole = y * m_holes_dist/2;
            // "ушки" крепления
            translate([0, y_hole, z_top - a_wall_width/2 - 1]) cylinder(h=2, d=m_holes_diam_out);
            // отверстия под болтики
            translate([0, y_hole, a_wall_width]) cylinder(h=z_top, d=m_holes_diam_in);
            // отверстия снизу под гайки M3(M4) под М3х10
            for (a=[-120, 0, 120]) translate([0, y_hole, z_screw]) rotate([0, 0,  a]) cube([h_screw_S, h_screw_E/2, h_screw_height], center=true);
            // небольшой конус от отверстия с гайкой, чтобы не делать поддержку
            translate([0, y_hole, z_screw  + h_screw_height/2]) cylinder(h=1.5, d2=m_holes_diam_in, d1 = h_screw_S);
        }
    }
}


// Крышка
translate([0, 0, 50])difference() { // z=20 чтобы впритык к корпусу
    c_height = 1 + 2 * a_rounding;
    a_width_no_rounding = a_width - 2*a_rounding;
    union() {
        // верх
        difference() {
            tmp_h = c_height - 2 * a_rounding;
            tmp_l = a_length - a_width/2 - a_rounding;
            translate([0, 0, a_rounding]) minkowski() {
                union() {
                    cylinder(h=tmp_h, d=a_width_no_rounding);
                    translate([tmp_l/2, 0, tmp_h/2]) cube([tmp_l, a_width_no_rounding, tmp_h], center = true);
                }
                sphere(r = a_rounding);
            }
            // отрезаем низ
            translate([0, 0, -0.5]) cylinder(h=c_height - a_rounding/2 + 0.5, d=3*a_width);
        }
        // выемка под корпус в половину толщины стенки
        translate([0, 0, c_height - a_wall_width]) {
            cylinder(h=a_wall_width/2, d=a_width_no_rounding + a_wall_width);
            tmp_l = a_length - a_width/2 - a_rounding;
            translate([tmp_l/2, 0, a_wall_width/4]) cube([tmp_l, a_width_no_rounding + a_wall_width, a_wall_width/2], center = true);
            // вырез под крепление второго (верхнего) IR
            if (IR_SENS_2) rotate([0, 0, IR_SENS_2_ANGLE]) translate([-a_width/2, 0, a_wall_width/4]) 
                cube([ir_depth - 2*ir_wall, ir_width, a_wall_width/2], center=true);
        }
        // крепления крышки
        y_offset = a_width_no_rounding/2 - c_latch_border_dist - c_latch_width/2;
        z_top = a_wall_width;
        for (y = [-1, 1]) translate([a_length - a_width/2 - c_latch_len - a_wall_width/3, y * y_offset, z_top + c_latch_height/2]) {
            rotate([90, 0, 90]) hole(c_latch_width - c_latch_height, c_latch_height, c_latch_len);
            translate([c_latch_len/2, 0, c_latch_height/4]) cube([c_latch_len, c_latch_width, c_latch_height/2], center=true);
            translate([-c_latch_height/4, 0, c_latch_height/4]) rotate([90, 0, 0]) round_arc(c_latch_height/2, c_latch_width, -1, -1);
        }
        // штырёк для кнопки нажатия на Boot
        translate([a_length - a_width/2 - b_btn_shift_x, b_btn_shift_y, c_height - a_wall_width  - b_btn_pin_height + b_btn_pin_diam/2]) {
            cylinder(h=b_btn_pin_height - b_btn_pin_diam/2 + 0.1, d=b_btn_pin_diam);
            sphere(d=b_btn_pin_diam);
        }
    }
    // вырез под мотор
    translate([m_center_to_shaft + m_shift_x, 0, -1]) cylinder(h=a_height, d=9.5);
    // вырезы под болтики
    for (y = [-1, 1]) {
        yy = y*m_holes_dist/2;
        translate([m_shift_x, yy, -0.5]) cylinder(h=a_height, d=m_holes_diam_in);
        translate([m_shift_x, yy, c_height - c_holes_height]) cylinder(h=c_holes_height + 0.1, d2=h_screw_E, d1=m_holes_diam_in);
    }
    // кнопка для нажатия на Boot
    translate([a_length - a_width/2 - b_btn_shift_x, b_btn_shift_y, -0.5]) {
        rotate([0, 0, -90]) cut_button(9, c_height+1, 8, 5, 1.2);
    }
}

// Мотор
%translate([m_shift_x, 0, a_wall_width + m_height/2 + 1]) import("Step_28BYJ-48.stl");

// Плата
%translate([a_length - a_width/2 - a_wall_width, b_width/2, a_wall_width + b_z_offset])
rotate([0,0,270]) scale(0.2573) translate([-3973.36, 3682 -132, 0]) import("D:/Devel/Projects/openscad/temp/OBJ_PCB_StepperRolls_4.obj.stl");

// второй (верхний) IR
if (IR_SENS_2) rotate([0, 0, IR_SENS_2_ANGLE]) translate([-a_width/2 + ir_wall, 0, a_height - ir_height - a_wall_width/2])
    %rotate([0,0,-90]) scale(0.2573) translate([-4020, 3421, -13]) import("VS1838B.stl");


module hole(dist_cent, diam, height) {
  hull() {
    translate([-dist_cent/2, 0, 0]) cylinder(h=height, d=diam);
    translate([dist_cent/2, 0, 0]) cylinder(h=height, d=diam);
  }
}

module round_arc(diam, width, dir_x = 1, dir_y = 1) {
    difference() {
        cube([diam, diam, width], center=true);
        translate([dir_x*diam/2, dir_y*diam/2, -width/2 - 0.5]) cylinder(h=width+1, d=2*diam);
    }
}

module cut_button(dist_cent, height, diam, base, thick) {
  difference() {
    union() {
      cylinder(h=height, d=diam);
      translate([dist_cent/2, 0, height/2]) cube([dist_cent, base, height], center = true);
    }
    translate([0, 0, -0.5]) cylinder(h=height +1, d=diam-thick);
    translate([dist_cent/2, 0, height/2]) cube([dist_cent +1, base - thick, height +1], center = true);
  }
}

module latch1(width1, height1, depth1, depth2, decrement) {
    latch2(width1, height1, depth1, width1 - decrement, height1 - decrement, depth2);
}

module latch2(width1, height1, depth1, width2, height2, depth2) {
  hull() {
    translate([0, depth1/2, 0]) cube([width1, depth1, height1], center=true);
    translate([0, depth1 + depth2/2, 0]) cube([width2, depth2, height2], center=true);
  }
}
