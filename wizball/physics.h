#ifndef _PHYSICS_H_
#define _PHYSICS_H_



#define PHYSICS_CONSTRAINT_BOOL_MIN			(1)
	// If this is set then the constraint will attempt to expand when in compression
#define PHYSICS_CONSTRAINT_BOOL_MAX			(2)
	// If this is set then the constraint will attempt to shrink when in tension

typedef struct 
{
	int id_type;
		// This allows you to set it with an constant, such as "PISTON" and then
		// you can set all pistons to be 150% normal size or the like within the
		// structure, effectively building an engine.
	int particle_1;
	int particle_2;
		// These are the two particles at either end of the constraint.
	float base_length;
		// This is how long the constraint is usually at 100%.
	float length;
		// This is how long it actually wants to be (as it may have been altered at your request)
	float current_length;
		// What it is at the moment.
	float base_elasticity;
		// This is how elastic it usually is at 100%.
	float elasticity;
		// And again this is how elastic it actually is (ie, the amount of length as a percentage it restores per frame).
	float base_friction;
		// How grippy it is at 100%.
	float friction;
		// How grippy it actually is.
	int type_flag;
		// This is a boolean flag made of the PHYSICS_CONSTRAINT_BOOL_MIN and PHYSICS_CONSTRAINT_BOOL_MAX defines.
	bool solid;
		// If this is set to true then the vector is used during collisions.
	float compressive_limit;
	float tensile_limit;
		// If either of these percentage deviations are exceeded then the health of the constraint
		// is depreciated by however far over the limit it is. By default the compressive limit
		// is 50% of the length and the tensile limit is 200% of the length.
	float maximum_health;
		// What the health starts at.
	float health;
		// What the health actually is. If it reaches 0 then the constraint SNAPS!
} constraint_structure;



typedef struct 
{
	int id_type;
		// This allows you to set it with an constant, such as "WHEEL" and then
		// you can set all wheels to be 150% normal size or the like within the structure.
	float current_x;
	float current_y;
		// Where the particle is...
	float old_x;
	float old_y;
		// Where the particle was...
	float mass;
		// How much it weighs and is used with constraints to bias movement
		// a value of float(UNSET) means infinite weight - ie, immovable.
	float base_radius;
		// How big the particle is at 100% size. Usually 0.0f in most cases.
	float radius;
		// How big it really is.
	bool apply_angular_momentum;
		// See below...
	float angle;
		// If the above is true then fake angular momentum is applied.
	float angular_velocity;
		// How fast it's rotating...
	float base_friction;
		// How grippy it is at 100%.
	float friction;
		// How grippy it actually is.
	bool solid;
		// If this is set to true then the particle is collided against the scenery and
		// and physics object vectors which aren't part of it's own structure.
} particle_structure;



typedef struct 
{
	int handle_constraint;
		// If this is != UNSET then it's the index of the constraint within the structure
		// which is used to update where the parent entity is. The position is said to be
		// the average position of the two particles at either end of the constraint.
	int handle_particle;
		// As above, really. If neither are set then the position is said to be the average
		// of all the particles within the structure.
	particle_structure *particle_list;
		// The list of particles which form the physics objects.
	int particle_count;
		// How many particles in the list?
	constraint_structure *constraint_list;
		// The list of constraints which form the physics objects.
	int constraint_count;
		// How many constraints in the list?
} physics_object_structure;



#endif
