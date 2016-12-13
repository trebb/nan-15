h_key = dxf_dim(file="frame.dxf", name="key",
                layer="side-view", origin=[0, 0], scale=1);
h_top_plate = dxf_dim(file="frame.dxf", name="tpl",
                      layer="side-view", origin=[0, 0], scale=1);
h_mid = dxf_dim(file="frame.dxf", name="mid",
                layer="side-view", origin=[0, 0], scale=1);
h_bottom_plate = dxf_dim(file="frame.dxf", name="bpl",
                         layer="side-view", origin=[0, 0], scale=1);
h_label_clearance = dxf_dim(file="frame.dxf", name="lbl",
                layer="side-view", origin=[0, 0], scale=1);
x0_cable = dxf_dim(file="frame.dxf", name="x0cable",
                layer="cable", origin=[0, 0], scale=1);
y0_cable = dxf_dim(file="frame.dxf", name="y0cable",
                layer="cable", origin=[0, 0], scale=1);
delta_cx = dxf_dim(file="frame.dxf", name="delta_cx",
                   layer="cable", origin=[0, 0], scale=1);
delta_cy = dxf_dim(file="frame.dxf", name="delta_cy",
                   layer="cable", origin=[0, 0], scale=1);
nan_15_h = dxf_dim(file="frame.dxf", name="nan-15-h",
                   layer="side-view", origin=[0, 0], scale=1);
epsilon = 1;

difference() {
  union() {
    difference() {
      color("green", $fa = 5, $fs = 0.2) {
        translate([0, 0, 0]) {
          difference() {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
            translate([-50, 0, -50]) {
              cube(100);
            }
            translate([0, -50, -50]) {
              cube(100);
            }
          }
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
          rotate([90, 0, 90]) {
            linear_extrude(height = delta_cx, $fa = 1, $fs = 0.2) {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
          }
        }
        translate([0, delta_cy, 0]) {
          difference() {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
            translate([-50, -100, -50]) {
              cube(100);
            }
            translate([0, -50, -50]) {
              cube(100);
            }
          }
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
          rotate([90, 0, 0]) {
            linear_extrude(height = delta_cy) {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
          }
        }
        translate([delta_cx, 0, 0]) {
          difference() {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
            translate([-50, 0, -50]) {
              cube(100);
            }
            translate([-100, -50, -50]) {
              cube(100);
            }
          }
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
          rotate([90, 0, 180]) {
            linear_extrude(height = delta_cy) {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
          }
        }
        translate([delta_cx, delta_cy, 0]) {
          difference() {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
            translate([-50, -100, -50]) {
              cube(100);
            }
            translate([-100, -50, -50]) {
              cube(100);
            }
          }
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
          rotate([90, 0, 270]) {
            linear_extrude(height = delta_cx) {
              import(file = "frame.dxf", layer = "outline-profile-2");
            }
          }
        }

        translate([x0_cable - 7.5, delta_cy, 0]) {
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
        }
        translate([x0_cable + 7.5, delta_cy, 0]) {
          rotate_extrude() {
            import(file = "frame.dxf", layer = "profile-2-handle");
          }
        }
        translate([x0_cable + 7.5, delta_cy, 0]) {
          rotate([90, 0, 270]) {
            linear_extrude(height = 15) {
              import(file = "frame.dxf", layer = "profile-2-handle");
            }
          }
        }

        for(i = [-21:7:21]) {
          translate([delta_cx, delta_cy / 2 + i , 0]) {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "profile-2-handle");
            }
          }
          translate([0, delta_cy / 2 + i , 0]) {
            rotate_extrude() {
              import(file = "frame.dxf", layer = "profile-2-handle");
            }
          }
        }

      }
      translate([0, 0, -50]) {
        hull() {
          linear_extrude(height = 100) {
            import(file = "frame.dxf", layer = "plate-support");
          }
        }
      }
      translate([delta_cx - x0_cable + 6, delta_cy + 1.8, nan_15_h]) {
        rotate([90, 0, 180]) {
          linear_extrude(height = 2) {
            text("NaN-15", halign = "left", valign = "center", font = "DejaVu Sans:style=Bold", size = 4, spacing = 1.1);
          }
        }
      }
    }
    translate([0, 0, 0]) {
      linear_extrude(height = h_mid) {
        import(file = "frame.dxf", layer = "plate-support");
      }
    }
    translate([0, 0, 0]) {
      linear_extrude(height = h_mid - 1) {
        import(file = "frame.dxf", layer = "bottom-support");
      }
    }
  } /* union */
  color("cyan", $fa = 5, $fs = 0.2) {
    translate([x0_cable, y0_cable, 0]) {
      rotate([-90, 0, 0]) {
        rotate_extrude() {
          import(file = "frame.dxf", layer = "cable-shape");
        }
      }
    }
  }
  translate([x0_cable, y0_cable + 15, 0]) {
    rotate([90, 0, 0]) {
      linear_extrude(height = 30) {
        import(file = "frame.dxf", layer = "cable-chamfer");
      }
    }
  }
}
