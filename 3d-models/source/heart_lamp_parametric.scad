// Parametric heart lamp: shade with custom names, date and a hidden message,
// plus top cap and a base that holds a 16-LED WS2812B ring (68 mm).
// Print in white PLA Matte, one part per plate. Default font: Pacifico (SIL OFL).
// Left and right are meant looking at the tip of the heart.

/* [Part] */
// Which part to generate. Print each part on its own plate.
part = "tube"; // [tube:Shade (tube), cap:Top cap, base:Base with LED ring seat, all:All three (preview only - does not fit one plate)]

/* [Outside text] */
// Name on the left side (looking at the tip of the heart)
left_name = "Anna";
// Name on the right side
right_name = "Luca";
// Small text under the left name (e.g. day and month)
left_date = "14·02";
// Small text under the right name (e.g. year)
right_date = "2025";
// Font of all the texts. Script fonts with joined letters look best (Pacifico, Lobster, Dancing Script...)
font = "Pacifico"; // font
// Height of the names (mm). About 6-7 letters fit at 14 mm: for longer names lower this value (text past the tip is cut)
name_size = 14; // [8:0.5:24]
// Height of the small texts (mm)
date_size = 11.5; // [6:0.5:20]
// Height of the names' baseline from the bottom of the shade (mm)
name_z = 29; // [10:1:120]
// Height of the small texts' baseline from the bottom of the shade (mm)
date_z = 16; // [6:1:120]
// How much the outside letters stick out (mm)
text_relief = 0.8; // [0.4:0.1:1.6]

/* [Secret message inside] */
// Show a hidden message on the inside wall (visible only when the lamp is on)
inner_text_on = true;
// Inside message, left part
inner_left = "love";
// Inside message, right part
inner_right = "you";
// Height of the inside letters (mm)
inner_size = 22.5; // [10:0.5:40]
// Height of the inside message baseline (mm, -1 = automatic, around half height)
inner_z = -1;
// How much the inside letters stick inward (mm)
inner_relief = 1.5; // [0.6:0.1:2.5]

/* [Shape] */
// Size of the heart: side of the square the heart is built on (mm). 62.5 gives about 107 x 96 mm
heart_size = 62.5; // [45:0.5:75]
// Height of the shade (mm)
height = 149; // [80:1:175]
// Wall thickness (mm). 0.8 = two 0.4 mm walls, lets a lot of light through
wall = 0.8; // [0.8:0.2:1.6]
// How much the wall bulges (0.05 = +5%)
wave_amount = 0.05; // [0:0.01:0.12]
// Number of bulges along the height
wave_count = 2; // [0:1:5]
// Rounding of the heart tip (mm)
tip_round = 4; // [1:0.5:8]
// Rounding of the dip between the two lobes (mm)
dip_round = 4; // [1:0.5:8]

/* [Cap and base] */
// Cap plate thickness (mm)
cap_thickness = 1.2; // [0.8:0.2:3]
// Height of the cap rim that goes inside the shade (mm)
cap_rim_height = 4; // [2:0.5:8]
// Thickness of the cap rim (mm)
cap_rim_thickness = 1.2; // [0.8:0.2:2]
// Gap between cap rim and shade (mm). Bigger = looser
cap_gap = 0.25; // [0:0.05:0.6]
// Base thickness (mm)
base_thickness = 5; // [3:0.5:10]
// Gap between base and shade (mm). Bigger = looser
base_gap = 0.3; // [0:0.05:0.6]
// Outer diameter of the LED ring (mm)
ring_diameter = 68; // [40:1:90]
// Extra room around the LED ring in its seat (mm)
ring_clearance = 1.2; // [0:0.1:3]
// Depth of the LED ring seat (mm)
ring_seat_depth = 1; // [0:0.5:3]
// Diameter of the center hole for the wires (mm)
wire_hole = 12; // [0:1:25]
// Width of the groove under the ring that leads the wires to the hole (mm, 0 = none)
wire_groove_width = 14; // [0:1:25]
// Depth of that groove (mm)
wire_groove_depth = 2; // [0:0.5:4]

/* [Quality] */
// Number of slices along the height (more = smoother bulges, slower)
wave_steps = 60; // [20:5:150]
// Points on each lobe of the heart
lobe_points = 64; // [24:4:128]

/* [Hidden] */
$fn = 64;
eps = 0.01;
s2 = sqrt(2);
d = heart_size / s2;          // half diagonal of the base square
R = heart_size / 2;           // lobe radius
yF = d/2 + sqrt(pow(R + dip_round, 2) - d*d/4);   // center of the dip rounding
// Ring center: same clearance to the flat sides and to the dip
yRing = (yF - dip_round - d/s2) / (1 + 1/s2);
h0 = yRing/s2 + d/s2;         // distance of each flat side from the ring center
inner_base = inner_z >= 0 ? inner_z : height/2 - inner_size/5;

// ---------- 2D heart outline, offset by "off" (positive = outward) ----------
function heart_pts(off) =
  let(
    nt = 10, nf = 12, nl = lobe_points,
    T  = [0, -d + tip_round*s2], rT = max(0.05, tip_round + off),
    F  = [0, yF],                rF = max(0.05, dip_round - off),
    rL = R + off,
    phi = atan2(yF - d/2, -d/2),
    psi = atan2(d/2 - yF, d/2),
    tip   = [for (i=[0:nt]) let(t = -135 + 90*i/nt) T + rT*[cos(t), sin(t)]],
    right = [for (i=[0:nl]) let(t = -45 + (phi + 45)*i/nl) [d/2, d/2] + rL*[cos(t), sin(t)]],
    dip   = [for (i=[1:nf-1]) let(t = psi + (-180 - 2*psi)*i/nf) F + rF*[cos(t), sin(t)]],
    left  = [for (i=[0:nl]) let(t = (180 - phi) + (225 - (180 - phi))*i/nl) [-d/2, d/2] + rL*[cos(t), sin(t)]]
  )
  [for (p = concat(tip, right, dip, left)) p - [0, yRing]];

module heart2d(off = 0) { polygon(heart_pts(off)); }

function wave(z) = 1 + wave_amount * pow(sin(180 * wave_count * z / height), 2);

// ---------- solid heart prism with the bulging wall, between z0 and z1 ----------
function layers(off, z0, z1, n) =
  let(P = heart_pts(off))
  [for (k=[0:n]) let(z = z0 + (z1 - z0)*k/n, s = wave(z)) for (p = P) [p[0]*s, p[1]*s, z]];

module wavy_solid(off, z0 = 0, z1 = height, n = wave_steps) {
  M = len(heart_pts(off));
  C = (n + 1) * M;   // two extra points: centers of bottom and top
  pts = concat(layers(off, z0, z1, n), [[0, 0, z0], [0, 0, z1]]);
  polyhedron(points = pts, faces = concat(
    [for (i=[0:M-1]) [C, i, (i+1)%M]],
    [for (i=[0:M-1]) [C + 1, n*M + (i+1)%M, n*M + i]],
    [for (k=[0:n-1]) for (i=[0:M-1]) each [
      [k*M + i, (k+1)*M + i, (k+1)*M + (i+1)%M],
      [k*M + i, (k+1)*M + (i+1)%M, k*M + (i+1)%M]]]
  ), convexity = 6);
}

// ---------- hollow shade as one polyhedron ----------
module shade_shell() {
  n = wave_steps;
  M = len(heart_pts(0));
  Q = (n + 1) * M;
  pts = concat(layers(0, 0, height, n), layers(-wall, 0, height, n));
  polyhedron(points = pts, faces = concat(
    [for (k=[0:n-1]) for (i=[0:M-1]) each [
      [k*M + i, (k+1)*M + i, (k+1)*M + (i+1)%M],
      [k*M + i, (k+1)*M + (i+1)%M, k*M + (i+1)%M]]],
    [for (k=[0:n-1]) for (i=[0:M-1]) each [
      [Q + k*M + i, Q + k*M + (i+1)%M, Q + (k+1)*M + (i+1)%M],
      [Q + k*M + i, Q + (k+1)*M + (i+1)%M, Q + (k+1)*M + i]]],
    [for (i=[0:M-1]) each [[i, (i+1)%M, Q + (i+1)%M], [i, Q + (i+1)%M, Q + i]]],
    [for (i=[0:M-1]) each [[n*M + i, Q + n*M + i, Q + n*M + (i+1)%M], [n*M + i, Q + n*M + (i+1)%M, n*M + (i+1)%M]]]
  ), convexity = 6);
}

// ---------- text projected onto one flat side ----------
// side = -1 left, +1 right (looking at the tip). valign_mode: "baseline", "bottom" or "center"
module side_text_prism(side, txt, size, z, valign_mode) {
  w_mid = -side * yRing / s2;
  // usable length of the flat side: text that is too long is cut near the tip
  w_tip  = w_mid - side * (heart_size/2 - tip_round * 0.6);
  w_lobe = w_mid + side * (heart_size/2 + 8);
  rotate([0, 0, side * 45]) rotate([90, 0, 0])
    translate([0, 0, h0 - 14])
      linear_extrude(height = 14 + h0 * wave_amount + 4)
        intersection() {
          translate([w_mid, z])
            text(txt, size = size, font = font, halign = "center", valign = valign_mode, $fn = 24);
          translate([min(w_tip, w_lobe), -1000]) square([abs(w_lobe - w_tip), 2000]);
        }
}

module outside_letters(side, txt, size, z) {
  if (len(txt) > 0) {
    zt0 = max(0, z - size * 0.6); zt1 = min(height, z + size * 1.8);
    difference() {
      intersection() {
        side_text_prism(side, txt, size, z, "baseline");
        wavy_solid(text_relief, zt0, zt1, 12);
      }
      translate([0, 0, -eps]) wavy_solid(-wall + 0.2, zt0 - 1, zt1 + 1, 12);
    }
  }
}

module inside_letters(side, txt, size, z) {
  if (len(txt) > 0) {
    zt0 = max(0, z - size * 0.6); zt1 = min(height, z + size * 1.8);
    difference() {
      intersection() {
        side_text_prism(side, txt, size, z, "baseline");
        wavy_solid(-wall + 0.2, zt0, zt1, 12);
      }
      wavy_solid(-wall - inner_relief, zt0 - 1, zt1 + 1, 12);
    }
  }
}

module tube() {
  union() {
    shade_shell();
    outside_letters(-1, left_name,  name_size, name_z);
    outside_letters( 1, right_name, name_size, name_z);
    outside_letters(-1, left_date,  date_size, date_z);
    outside_letters( 1, right_date, date_size, date_z);
    if (inner_text_on) {
      inside_letters(-1, inner_left,  inner_size, inner_base);
      inside_letters( 1, inner_right, inner_size, inner_base);
    }
  }
}

module cap() {
  s = wave(height);
  scale([s, s, 1]) {
    linear_extrude(cap_thickness) heart2d(0);
    translate([0, 0, cap_thickness - eps])
      linear_extrude(cap_rim_height + eps)
        difference() {
          heart2d(-wall - cap_gap);
          heart2d(-wall - cap_gap - cap_rim_thickness);
        }
  }
}

module base() {
  difference() {
    linear_extrude(base_thickness) heart2d(-wall - base_gap);
    // seat for the LED ring
    if (ring_seat_depth > 0)
      translate([0, 0, base_thickness - ring_seat_depth])
        cylinder(d = ring_diameter + ring_clearance, h = ring_seat_depth + 1, $fn = 128);
    // wire hole
    if (wire_hole > 0)
      translate([0, 0, -1]) cylinder(d = wire_hole, h = base_thickness + 2);
    // groove under the ring for the wires
    if (wire_groove_width > 0 && wire_groove_depth > 0)
      translate([-wire_hole/2, -wire_groove_width/2, base_thickness - ring_seat_depth - wire_groove_depth])
        cube([wire_hole/2 + (ring_diameter + ring_clearance)/2, wire_groove_width, wire_groove_depth + eps]);
  }
}

// ---------- layout ----------
spacing = heart_size * 1.9;
if (part == "tube") tube();
else if (part == "cap") cap();
else if (part == "base") base();
else {
  tube();
  translate([spacing, 0, 0]) cap();
  translate([2 * spacing, 0, 0]) base();
}
