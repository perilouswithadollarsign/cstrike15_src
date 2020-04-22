#ifndef POINT_H
#define POINT_H

class point
{
public:
	long x, y;

	point( void ) { ; };
	point( point const & p ) { x = p.x; y = p.y; };
	point( POINT const & p ) { x = p.x; y = p.y; };

	bool operator <  ( point const & r ) const { return x < r.x || (x == r.x && y < r.y); };
	bool operator >  ( point const & r ) const { return !(*this < r); };
	bool operator == ( point const & r ) const { return x == r.x && y == r.y; };
	bool operator != ( point const & r ) const { return !(*this == r); };
};

#endif POINT_H