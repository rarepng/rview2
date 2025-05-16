in vector<float, 3> aPos;
in vector<float, 3> aNomral;
in vector<float, 2> aTexCoord;

out vector<float, 3> normal;
out vector<float, 2> texCoord;
out vector<float, 4> newcolor;
out uint32_t txidx;

// [push_constant]
// uniform Constants {
//     int aModelStride;
//     uint txid;
//     float t;
//     bool decaying;
// }