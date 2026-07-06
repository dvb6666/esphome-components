$fn = 100;

// основание
b_centers_dist = 60; // расстояние между центрами отверстий
b_w1 = 10; // ширина
b_h1 = 2; // высота
// отверстия
s_d1 = 3; // диаметр отверстия
s_h1 = 5; // высота цилиндра над основанием
// "ушки"
u_w1 = 14.8;
u_w2 = 18;
u_d1 = 8;

// M3 винт
h_screw_S = 5.5;
h_screw_E = 6.35;
h_screw_height = 2.3;

d2 = 2*s_d1;
h0 = b_h1 + s_h1;
difference() {
    union() {
        translate([0, 0, b_h1/2]) cube([b_centers_dist, b_w1, b_h1], center = true);
        for (x = [-1, 1]) {
            translate([x*(b_centers_dist/2 - b_w1/2 - u_d1/2), 0, 0]) {
                translate([0, 0, b_h1/2]) cube([u_d1, u_w2, b_h1], center = true);
                for (y = [-1, 1]) translate([0, y*(u_w1-b_h1)/2, b_h1+b_h1/2]) cube([u_d1, b_h1, b_h1], center = true);
            }
            translate([x*(b_centers_dist/2), 0, 0]) {
                cylinder(h=b_h1+1, d=b_w1);
                cylinder(h=h0, d=d2);
                translate([0, 0, b_h1+1]) cylinder(h=b_h1, d1=b_w1, d2=d2);
            }
        }
    }
    for (x = [-1, 1]) {
        translate([x*(b_centers_dist/2), 0, 0]) {
            translate([0, 0, -0.5]) cylinder(h=h0 + 1, d=s_d1);
            translate([0, 0, -0.1 + h_screw_height/2]) for (a=[-120, 0, 120]) rotate([0, 0,  90+a]) cube([h_screw_S, h_screw_E/2, h_screw_height], center=true);
            translate([0, 0, -0.2 + h_screw_height]) cylinder(h=1, d1=h_screw_S, d2=s_d1);
        }
    }
}
