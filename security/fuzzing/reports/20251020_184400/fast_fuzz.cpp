#include <cstdint>
#include <cstring>
#include <cstdio>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 1000) return 0;
    uint8_t buffer[256];
    size_t copy_size = (size < sizeof(buffer)) ? size : sizeof(buffer);
    memcpy(buffer, data, copy_size);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 1000) { fclose(f); return 1; }
    uint8_t* data = new uint8_t[size];
    fread(data, 1, size, f);
    fclose(f);
    int result = LLVMFuzzerTestOneInput(data, size);
    delete[] data;
    return result;
}
