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
