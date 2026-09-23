#ifndef ENGINE64_TIME_H
#define ENGINE64_TIME_H

namespace e64 {

class Time {
public:

	float counter;
	float delta;
	float rate;

};


namespace time {

Time *get(void);

void init(void);
void update(void);
void setScale(float scale);

/* After a blocking load: drops the time it took, so the next frame's delta
   is a normal one instead of the whole load measured as gameplay. */
void reset(void);

}

}

#endif
