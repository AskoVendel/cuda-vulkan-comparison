[[vk::binding(0, 0)]] RWStructuredBuffer<int> d_data;
[[vk::binding(1, 0)]] RWStructuredBuffer<int> d_out1;
[[vk::binding(2, 0)]] RWStructuredBuffer<int> d_coeff;


struct Params
{
    int width;
    int height;
    int N;
    float divider;
};

[[vk::push_constant]]
Params constants;



[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint x = DTid.x;
    uint y = DTid.y;

    if (x >= constants.width || y >= constants.height)
        return;

    for (int c = 0; c < 3; c++)
    { // RGB
        uint index = (y * constants.width + x) * 4 + c;

        if (y < constants.N || x < constants.N || y >= (constants.height - constants.N) || x >= (constants.width - constants.N))
        {
            d_out1[index] = d_data[index];
            continue;
        }

        float sum = 0.0f;
        for (int a = -constants.N; a <= constants.N; a++)
        {
            for (int b = -constants.N; b <= constants.N; b++)
            {
                //uint nx = x + a;
                //uint ny = y + b;
                //uint neighbor_idx = (ny * constants.width + nx) * 4 + c;
                
                int neighbor_idx = ((y + b) * constants.width + (x + a)) * 4 + c;
                sum += d_data[neighbor_idx] * d_coeff[(abs(a) * (constants.N + 1)) + abs(b)];

                // Accessing the coefficient by (abs(a), abs(b)) as in the CUDA code
                //sum += d_data[neighbor_idx] * d_coeff[abs(a) * (constants.N + 1) + abs(b)];
            }
        }

        d_out1[index] = (int) (sum / constants.divider);
    }
}