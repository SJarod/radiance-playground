
/// DIRECT LIGHTING
/// SCENE

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.y;
    fragColor = vec4(0.0);
    
    // lights
    //lights[0].position.y = (sin(iTime / 2.0) + 1.0) / 2.0;
    //lights[2].position.x = (sin(iTime / 4.0) + 1.0) / 2.0;
    for (int i = 0; i < int(LIGHT_COUNT); ++i)
    {
        float lightRadius = lights[i].radius;
		fragColor += lights[i].color * draw_sphere(uv, lights[i].position, lightRadius, iResolution.y);
    }
    
    // black bar
    float bl = step(0.0, uv.x); // bottom left
    float br = step(0.0, 1.0 - uv.x); // bottom right
    float tl = step(0.17 + (sin(iTime / 10.0) + 1.0) / 2.0, uv.y); // top left
    float tr = step(0.8 - (sin(iTime / 10.0) + 1.0) / 2.0, 1.0 - uv.y); // top right
    float c = bl * br * tl * tr; // step color
    vec4 bar = vec4(0.0, 0.0, 0.0, 1.0) * c;
    //fragColor += bar;
    
    Light mouse = Light(iMouse.xy / iResolution.y, 0.05, vec4(vec3(0.0), 1.0));
    fragColor += mouse.color * draw_sphere(uv, mouse.position, mouse.radius, iResolution.y);
}