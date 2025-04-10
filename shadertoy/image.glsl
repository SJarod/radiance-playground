
// radiance cascades

// ref 0 : paper : https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/
// ref 1 : https://tmpvar.com/poc/radiance-cascades/
// ref 2 : https://radiance-cascades.com/

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    
    vec4 direct = texture(iChannel0, uv);
    vec4 indirect = texture(iChannel1, uv);
    fragColor = vec4(direct.rgb + mix(indirect.rgb, direct.rgb, step(0.5, direct.w)), 1.0);
    //fragColor = vec4(direct.rgb + indirect.rgb, 1.0);
}