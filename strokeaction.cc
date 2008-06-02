#include "strokeaction.h"

StrokeAction& stroke_action() {
	static StrokeAction sa;
	return sa;
}
