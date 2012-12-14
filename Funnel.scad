

funnel2();

//funnel1();

module funnel2() {
	difference() {
		union() {
			translate([0,0,24.5])
				cylinder(r=15, h=20);
			cylinder(r1=25, r2=15, h=25);
		}
		translate([0,0,-6])
			cylinder(r1=25, r2=10, h=35);
		cylinder(r=3.5, h=70);
	}
	translate([-16,-16,0])
		cube([32, 32, 0.5]);
}

module funnel1() {
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
}
