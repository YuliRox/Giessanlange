// Mounting plate for PC 240011 G 5U enclosure (Unterteil)
// Screws onto all box bosses; carries Revolt NX-6815 controller + breadboard.
//
// ── All dimensions in mm. Edit parameters only — no numbers elsewhere. ───────

// ── Box reference (drawing 5U240011 G) ───────────────────────────────────────
box_outer_w         = 239;      // 240.3 nominal / 239 actual
box_outer_d         = 159;      // 160.3 nominal / 159 actual
box_inner_w         = 233;      // inner wall-to-wall, long axis  (234.3/233)
box_inner_d         = 153;      // inner wall-to-wall, short axis (154.3/153)
protrusion_span_h   = 216;      // clear span between corner protrusions, horizontal
protrusion_span_v   = 116.7;    // clear span between corner protrusions, vertical

// ── Corner protrusion cutouts (derived from drawing) ─────────────────────────
corner_cut_x    = (box_inner_w - protrusion_span_h) / 2;   // = 8.5 mm
corner_cut_y    = (box_inner_d - protrusion_span_v) / 2;   // = 18.15 mm
corner_margin   = 0.5;   // extra clearance added to each cutout side
corner_cut_r    = 6;     // inner fillet radius matching box R6
corner_chamfer  = 2.5;   // leg of the 45° chamfer at the outer step corners

// ── Plate geometry ────────────────────────────────────────────────────────────
plate_fit_margin = 1;    // total clearance so plate slides into box (split across both sides)
plate_w         = box_inner_w - plate_fit_margin;
plate_d         = box_inner_d - plate_fit_margin;
plate_t         = 3;
plate_corner_r  = 4;

// ── Mid-edge notches (top and bottom centre, lid-screw bumps) ─────────────────
mid_notch_depth = (box_inner_d - 136) / 2 + 1.5;   // = 16.5 mm + 1.5 clearance
mid_notch_width = 16;             // estimated — tune to match actual bump width
mid_notch_r     = 4;              // fillet radius on the notch corners

// ── Large bosses — all ø8.5 OD, ø4 inner, M4 clearance ──────────────────────
boss_clearance  = 4.5;

// Boss A — 4 outer-corner bosses
boss_a_h        = 201.4;   // C-C horizontal
boss_a_v        = 138;     // C-C vertical

// Boss B — 4 inner-corner bosses (at base of corner protrusions)
boss_b_h        = protrusion_span_h;   // = 216 mm C-C horizontal
boss_b_v        = 75;                  // C-C vertical

// Boss C — 2 centre wall bosses (one per side wall, at vertical centre Y = 0)
// Vertically aligned with Boss B (same X), centred between the two Boss B rows.
boss_c_x        = boss_b_h / 2;   // = 108 mm — same wall ridge as Boss B
boss_c_y        = 0;

// Small bosses flanking Boss C (ø3, no thread) and tiny centre bosses (ø2):
// these are solid support pegs — they bear the plate from below, no holes needed.

// ── Revolt NX-6815 controller — M4 clearance ─────────────────────────────────
ctrl_hole_dia   = 4.3;
ctrl_h_spacing  = 50;      // C-C horizontal (rotated 90°)
ctrl_v_spacing  = 126.5;   // C-C vertical   (rotated 90°)
ctrl_wall_gap   = 40;      // distance from left plate edge to nearest controller hole
ctrl_center_x   = -plate_w/2 + ctrl_wall_gap + ctrl_h_spacing/2;
ctrl_center_y   = 0;       // group centre offset from plate centre, Y

// ── Breadboard / ESP32 — M2.5 clearance ──────────────────────────────────────
// Breadboard oriented so long axis (155 mm) runs along plate_w.
bb_hole_dia     = 3.0;
bb_h_spacing    = 155;     // C-C along plate width  (long axis of breadboard)
bb_v_spacing    = 89;      // C-C along plate depth  (short axis)
bb_center_x     = 22;      // group centre offset from plate centre, X
bb_center_y     = 0;       // group centre offset from plate centre, Y

// ── Internal ──────────────────────────────────────────────────────────────────
eps = 0.5;
$fn = 64;

// ── Modules ───────────────────────────────────────────────────────────────────

// Solid rounded-rectangle plate, geometric centre at origin, Z = 0..plate_t
module rounded_plate() {
    translate([0, 0, plate_t / 2])
        minkowski() {
            cube([plate_w - 2*plate_corner_r,
                  plate_d - 2*plate_corner_r,
                  plate_t - eps], center = true);
            cylinder(r = plate_corner_r, h = eps);
        }
}

// 4 through-holes at (±h/2, ±v/2)
module hole_pattern(h, v, dia) {
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([sx * h/2, sy * v/2, -eps])
            cylinder(d = dia, h = plate_t + 2*eps);
}

// Single through-hole at (x, y)
module single_hole(x, y, dia) {
    translate([x, y, -eps])
        cylinder(d = dia, h = plate_t + 2*eps);
}

// Rounded-rectangle cutout, w × d, with inner fillet radius r, centred at origin
module rounded_cutout(w, d, r) {
    translate([0, 0, -eps])
        linear_extrude(height = plate_t + 2*eps)
            offset(r = r)
                square([max(w - 2*r, eps), max(d - 2*r, eps)], center = true);
}

// ── Assembly ──────────────────────────────────────────────────────────────────
difference() {
    rounded_plate();

    // Boss A: 4 outer-corner bosses
    hole_pattern(boss_a_h, boss_a_v, boss_clearance);

    // Boss B: 4 inner-corner bosses (at protrusion base)
    hole_pattern(boss_b_h, boss_b_v, boss_clearance);

    // Boss C: 2 centre wall bosses
    single_hole(-boss_c_x, boss_c_y, boss_clearance);
    single_hole(+boss_c_x, boss_c_y, boss_clearance);

    // Revolt NX-6815 controller mounting holes
    translate([ctrl_center_x, ctrl_center_y, 0])
        hole_pattern(ctrl_h_spacing, ctrl_v_spacing, ctrl_hole_dia);

    // Breadboard mounting holes
    translate([bb_center_x, bb_center_y, 0])
        hole_pattern(bb_h_spacing, bb_v_spacing, bb_hole_dia);

    // Mid-edge notches: top and bottom centre lid-screw bumps
    // Centred on plate edge so outer half falls outside plate — only inner corners are rounded.
    for (sy = [-1, 1])
        translate([0, sy * plate_d/2, 0])
            rounded_cutout(mid_notch_width, 2*mid_notch_depth, mid_notch_r);

    // Corner protrusion cutouts — centred on plate corner so outer faces fall outside
    // the plate and get clipped; only the single inner corner gets the fillet.
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([sx * plate_w/2, sy * plate_d/2, 0])
            rounded_cutout(2 * (corner_cut_x + corner_margin),
                           2 * (corner_cut_y + corner_margin),
                           corner_cut_r);

    // 45° chamfers at the 8 outer step corners: subtract a cube rotated 45° centred at each
    // junction. The parts of the cube outside the plate cut nothing; the overlapping quarter
    // removes the sharp corner. No winding issues.
    for (sx = [-1, 1]) for (sy = [-1, 1]) {
        // Junction: top/bottom plate edge meets cutout vertical wall
        translate([sx * (plate_w/2 - (corner_cut_x + corner_margin)),
                   sy * plate_d/2, plate_t/2])
            rotate([0, 0, 45])
            cube([corner_chamfer * sqrt(2), corner_chamfer * sqrt(2), plate_t + 2*eps],
                 center = true);
        // Junction: left/right plate edge meets cutout horizontal wall
        translate([sx * plate_w/2,
                   sy * (plate_d/2 - (corner_cut_y + corner_margin)), plate_t/2])
            rotate([0, 0, 45])
            cube([corner_chamfer * sqrt(2), corner_chamfer * sqrt(2), plate_t + 2*eps],
                 center = true);
    }
}
