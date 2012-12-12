

difference() {
	union() {
		translate([0,0,20])
			cylinder(r=5, h=20);
		difference() {
			#cylinder(r1=15, r2=1, h=35);
			translate([0,0,-5])
				cylinder(r1=15, r2=1, h=35);
			translate([-20,-20,25])
				cube(size = 40);
		}
	}
	translate([0,0,35])
		cylinder(r=2, h=10);
}