#version 330 core
in vec2 v_uv;
in vec4 v_col;
out vec4 o_color;
uniform sampler2D u_font;
void main() {
    if (v_uv.x < 0.0) {
        o_color = v_col;
    } else {
        float a = texture(u_font, v_uv).r;
        o_color = vec4(v_col.rgb, v_col.a * a);
    }
}
