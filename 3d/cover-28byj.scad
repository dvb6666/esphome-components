$fn = 100;

error=0.2;

bottom_to_shaft = 32.5; // расстояние от нижней границы до центра вала
bottom_clearence = 10;

// motor
m_center_to_shaft = 8; // расстояние от центра мотора до центра вала
m_height=20;
m_diam=28;
m_shift_from_center=0; // отступ корпуса от центра (вала)
m_holes_dist = 35; // расстояние между центрами "ушек" креплений
m_holes_height = 1 + error; // высота "ушек"
m_holes_diam_out = 7 + error; // внешний диаметр "ушек"
m_holes_diam_in = 3.2; // внутренний диаметр отверстий в "ушках"
// bottom box
b_shift_from_center=-4; // отступ корпуса от центра
b_rounding = 2; // скругление корпуса
b_length = bottom_to_shaft + b_shift_from_center - bottom_clearence;
b_diam = m_holes_dist + m_holes_diam_out + 3;//45
b_height = m_height;// + 4;
// top cover
t_height=2;
ts_diam=20;
ts_height=0.7;

// M3 винт
h_screw_S = 5.5;
h_screw_E = 6.35;
h_screw_height = 3;
h_screw_top_shift = 10 - t_height;//h_screw_height + 5;

// горизонтальная площадка со стороны крепления к окну
b_w1=35.6; // ширина базы в районе крепления
// крепление к окну
k_w1=2.8; // ширина крепления
k_h1=21.1; // высота крепления
k_r=3; // радиус скругления углов
kb_h1 = 5; // высота боковой стенки
translate([-bottom_to_shaft + m_center_to_shaft, 0, b_height - 10]) rotate([0,0,90]) translate([-b_w1/2, -k_w1, 0])
  difference() {
    kb_y = bottom_to_shaft - m_center_to_shaft - b_diam/2 + 2*b_rounding;
    kb_r = kb_y + k_w1;
    union() {
      cube([b_w1, k_w1, k_h1-k_r]);
      translate([k_r, 0, 0]) cube([b_w1-2*k_r, k_w1, k_h1]);
      translate([k_r, 0, k_h1-k_r]) rotate([0, 90,  90]) cylinder(h=k_w1, d=k_r*2);
      translate([b_w1-k_r, 0, k_h1-k_r]) rotate([0, 90,  90]) cylinder(h=k_w1, d=k_r*2);
      for (x = [0, 1])  translate([x*(b_w1-kb_h1), -kb_y, 0]) cube([kb_h1, kb_y, kb_h1]);
      // скругленные скаты
      difference(){
        translate([0, -kb_y, -kb_r]) cube([b_w1, kb_r, kb_r]);
        translate([b_w1/2, k_w1, -kb_r]) rotate([0, 90,  0]) cylinder(h=b_w1+1, r=kb_r, center=true);
        translate([kb_h1, -kb_y, -kb_r]) cube([b_w1-2*kb_h1, kb_r, kb_r]);
      }
    }
    // два задних продолговатых отверстия
    k_h_r=24; // расстояние по X
    k_h_h1=4; // расстояние между центрами отверстий
    k_h_d1=3.8; // диаметр отверстия
    k_h_sh=k_w1-0.8; // сдвиг по глубине отверстий; поставить "-0.1", чтобы отверстия стали скозными
    for (x = [-1, 1]) translate([b_w1/2 + x * k_h_r/2, k_h_sh, 1+k_h1/2]) rotate([0, 90,  90]) hole(k_h_h1, k_w1+0.2, k_h_d1);
  }

// площадка под мотор
b_dist_bottom = bottom_to_shaft + b_shift_from_center - m_center_to_shaft - b_rounding - bottom_clearence;
translate([0, 0, b_rounding]) 
difference() {
  m_diam2 = m_diam + error; // диаметр выреза под мотор с небольшим запасом
  minkowski() {
    union() {
      yy = b_diam-2*b_rounding;
      translate([b_shift_from_center, 0, 0]) difference() {cylinder(h=b_height, d=yy); translate([-yy/2, 0, 0]) cube([yy/2, b_diam, b_height*3], center = true);}
//      translate([-b_dist_bottom + b_shift_from_center, -yy/2, 0]) cube([b_dist_bottom, yy, b_height]);
    }
    sphere(r = b_rounding);
  }
  // отрезаем скругленный верх
  translate([0, 0, b_height]) cylinder(h=b_height, d=3*b_diam);
  
  // вырезаем отверстие под мотор
  translate([0, 0, -1]) cylinder(h=b_height+2, d=m_diam2);
  // вырез до основания
//  translate([-b_diam, 0, b_height/2+0.5]) cube([2*b_diam, m_diam2, b_height+1], center=true);
  m_diff = b_w1 - 2*kb_h1;
  translate([-b_diam, 0, b_height/2+0.5]) cube([2*b_diam, m_diff, b_height+1], center=true);
  
  // вырез между "ушками" креплений
  translate([-m_holes_diam_out/2, -m_holes_dist/2, b_height-m_holes_height]) cube([m_holes_diam_out, m_holes_dist, 100]);
  // вырез
  translate([-h_screw_S/2, -m_holes_dist/2, b_height-h_screw_top_shift-h_screw_height/2]) cube([h_screw_S, m_holes_dist, h_screw_height]);
  zz = b_height - h_screw_top_shift;
  for (y = [-1, 1]) {
    yy = y*m_holes_dist/2;
    // "ушки" крепления
    translate([0, yy, b_height - m_holes_height]) cylinder(h=m_holes_height+50, d=m_holes_diam_out);
    // отверстия под болтики
    translate([0, yy, zz - h_screw_height/2 - 2]) cylinder(h=h_screw_top_shift+50, d=m_holes_diam_in);
    // отверстия снизу под гайки M3(M4) под М3х10
    for (a=[-120, 0, 120]) translate([0, yy, zz]) rotate([0, 0,  a]) cube([h_screw_S, h_screw_E/2, h_screw_height], center=true);
    // небольшой конус от отверстия с гайкой, чтобы не делать поддержку
    translate([0, yy, zz  + h_screw_height/2]) cylinder(h=1.5, d2=m_holes_diam_in, d1 = h_screw_S);
    
    // старый вариант со сквозным отверстием  
//    // отверстия под болтики
//    translate([0, yy, -b_height]) cylinder(h=100, d=m_holes_diam_in);
//    // отверстия снизу под гайки M3(M4) под М3х20
//    h_screw_S = 5.5; //7;
//    h_screw_E = 6.35; //8.1;
//    h_screw_height = 5; // высота отступа снизу
//    for (a=[-120, 0, 120]) translate([0, yy, 0]) rotate([0, 0,  a+90]) cube([h_screw_S, h_screw_E/2, h_screw_height], center=true);
  }
}


// мотор
%color("grey") translate([0, 0, b_height - m_height/2 + b_rounding]) import("Step_28BYJ-48.stl");


// крышечка
translate([0, 0, 45])
difference() {
  m_diam2 = m_diam + error; // диаметр выреза под мотор с небольшим запасом
  minkowski() {
    union() {
      yy = b_diam-2*b_rounding;
      translate([b_shift_from_center, 0, 0]) difference() {cylinder(h=t_height, d=yy); translate([-yy/2, 0, 0]) cube([yy/2, b_diam, b_height*3], center = true);}
//      translate([-b_dist_bottom + b_shift_from_center, -yy/2, 0]) cube([b_dist_bottom, yy, t_height]);
    }
    sphere(r = b_rounding);
  }
  // отрезаем скругленный низ
  zz = t_height + b_rounding;
  translate([0, 0, -t_height]) cylinder(h=zz, d=3*b_diam);
  translate([m_center_to_shaft, 0, 0]) cylinder(h=100, d=9.5);
//  translate([m_center_to_shaft, 0, zz - ts_height]) cylinder(h=100, d=ts_diam);
  for (y = [-1, 1]) {
    yy = y*m_holes_dist/2;
    translate([0, yy, -1]) cylinder(h=100, d=m_holes_diam_in);
    translate([0, yy, zz - ts_height]) cylinder(h=100, d=h_screw_E);
  }
}


// колпачок
// диаметр, высота, высота верхушки
s_d1=15.6; s_len=20; s_cone=5;
s_d2=17.4; // диаметр вместе с ребрами
// диаметр (чуть шире основного цилиндра) и высота основания
s_bd=s_d1 + 4.4; s_bl=1.5;
translate([m_center_to_shaft, 0, 70])
difference() {
  // рисуем основную часть
  intersection() {
	union() {
      // нижнее основание
	  translate([0, 0, -s_bl]) cylinder(h=s_bl, d=s_bd);
      // "ровная" середина стержня
      delta = 0.6; // небольшой скос от основания к краю стержня
      cylinder(h=s_len-s_cone, d1=s_d1, d2=s_d1-delta);
      // "скошенная" верхушка стержня
	  translate([0, 0, s_len-s_cone]) cylinder(h=s_cone, d1=s_d1-delta, d2=s_d1-delta-s_cone/2);
      // рёбра
      for (z = [-60, 0, 60]) 
        rotate([0, 0,  z]) translate([0, 0, s_len/2]) cube([1.5, s_d2, s_len], center=true);
	}
    // делаем скос на рёбрах
	translate([0, 0, -s_bl-0.1]) cylinder(h=s_len+s_bl+0.2, d1=s_len*2, d2=s_d1-0.6-s_cone/2);
  }
  // в основании вырезаем полость под вал шагового моторчика
  sd=5.0; sw=3.0; sl=7;
  intersection() {
	translate([0, 0, -s_bl-0.1]) cylinder(h=sl+0.1, d=sd);
	translate([0, 0, sl/2-s_bl-0.1]) cube([sw, sd, sl+0.1], center=true);
  }
  // вырезаем центр стрежня (чуть выше выреза по вал моторчика)
  hl=sl + 3;
  translate([0, 0, hl]) cylinder(h=s_len, d=9);
}

module hole(dist_cent, height, diam) {
  hull() {
    translate([-dist_cent/2, 0, 0]) cylinder(h=height, d=diam);
    translate([dist_cent/2, 0, 0]) cylinder(h=height, d=diam);
  }
}
