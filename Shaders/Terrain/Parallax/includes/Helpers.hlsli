#define	expand(v)	        (((v) - 0.5) / 0.5)  // from 0/1 to -1/1
#define	compress(v)         (((v) * 0.5) + 0.5)  // from -1/1 to 0/1 
#define	shades(n, l)        saturate(dot(n, l))
#define	weight(v)           dot(v, 1)

float4 asm_lit(float4 src) {
    float4 dest;
    
    dest.x = 1;
    dest.y = 0;
    dest.z = 0;
    dest.w = 1;

    float power = src.w;
    const float MAXPOWER = 127.9961f;
    if (power < -MAXPOWER)
        power = -MAXPOWER; // Fits into 8.8 fixed point format
    else if (power > MAXPOWER)
        power = MAXPOWER; // Fits into 8.8 fixed point format

    if (src.x > 0) {
        dest.y = src.x;
        if (src.y > 0) {
        // Allowed approximation is EXP(power * LOG(src.y))
            dest.z = (float) (pow(src.y, power));
        }
    }
    
    return dest;
}