#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Byte-addressable buffers (as uints)
layout(set = 0, binding = 0) buffer InputBuffer {
    uint d_data[]; 
};

layout(set = 0, binding = 1) buffer OutputBuffer {
    uint d_out[]; 
};

layout(set = 0, binding = 2) buffer CoeffBuffer {
    float d_coeff[];
};

layout(push_constant) uniform Params {
    int width;
    int height;
    int N;
    float divider;
} constants;

uint read_byte(uint index) {
    uint word = d_data[index >> 2];               
    uint byte_shift = (index & 3u) * 8u;           
    return (word >> byte_shift) & 0xFFu;           
}

void write_byte(uint index, uint value) {
    uint word_index = index >> 2;
    uint byte_shift = (index & 3u) * 8u;

    uint old_word = d_out[word_index];
    uint mask = ~(0xFFu << byte_shift);
    uint new_word = (old_word & mask) | ((value & 0xFFu) << byte_shift);
    d_out[word_index] = new_word;
}

void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);

    if (x >= constants.width || y >= constants.height)
        return;

    for (int c = 0; c < 3; c++) {
        int index = (y * constants.width + x) * 4 + c;

        if (y < constants.N || x < constants.N ||
            y >= constants.height - constants.N || x >= constants.width - constants.N) {
            uint value = read_byte(uint(index));
            write_byte(uint(index), value);
            continue;
        }

        float sum = 0.0;
        for (int a = -constants.N; a <= constants.N; ++a) {
            for (int b = -constants.N; b <= constants.N; ++b) {
                int neighbor_idx = ((y + b) * constants.width + (x + a)) * 4 + c;
                uint pixel = read_byte(uint(neighbor_idx));
                int coeff_idx = abs(a) * (constants.N + 1) + abs(b);
                sum += float(pixel) * d_coeff[coeff_idx];
            }
        }

        uint result = uint(clamp(sum / constants.divider, 0.0, 255.0));
        write_byte(uint(index), result);
    }
}