#include <complex.h>

_Complex double __muldc3(double a, double b, double c, double d) {
	_Complex double lhs = a + b * I;
	_Complex double rhs = c + d * I;
	return lhs * rhs;
}

_Complex float __mulsc3(float a, float b, float c, float d) {
	_Complex float lhs = a + b * I;
	_Complex float rhs = c + d * I;
	return lhs * rhs;
}
