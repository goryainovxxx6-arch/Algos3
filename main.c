#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.bmp> <output.bmp>\n", argv[0]);
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    FILE* input = fopen(input_path, "rb");
    if (!input) {
        perror("Cannot open input file");
        return 1;
    }

    BMPHeader bmp_header;
    BMPInfoHeader info_header;

    if (fread(&bmp_header, sizeof(BMPHeader), 1, input) != 1) {
        fprintf(stderr, "Error reading BMP header\n");
        fclose(input);
        return 1;
    }

    if (bmp_header.bfType != 0x4D42) { // 'BM'
        fprintf(stderr, "Not a valid BMP file\n");
        fclose(input);
        return 1;
    }

    if (fread(&info_header, sizeof(BMPInfoHeader), 1, input) != 1) {
        fprintf(stderr, "Error reading BMP info header\n");
        fclose(input);
        return 1;
    }

    // Проверка: 24 бита, без сжатия
    if (info_header.biBitCount != 24 || info_header.biCompression != 0) {
        fprintf(stderr, "Only 24-bit uncompressed BMP supported\n");
        fclose(input);
        return 1;
    }

    int32_t width = info_header.biWidth;
    int32_t height = info_header.biHeight;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid image dimensions\n");
        fclose(input);
        return 1;
    }

    // Вычисляем размер строки с выравниванием до 4 байт
    int row_size = (width * 3 + 3) & ~3;
    size_t pixel_data_size = (size_t)row_size * height;

    // Перемещаемся к началу пиксельных данных
    if (fseek(input, bmp_header.bfOffBits, SEEK_SET) != 0) {
        fprintf(stderr, "Cannot seek to pixel data\n");
        fclose(input);
        return 1;
    }

    uint8_t* pixel_data = malloc(pixel_data_size);
    if (!pixel_data) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(input);
        return 1;
    }

    if (fread(pixel_data, 1, pixel_data_size, input) != pixel_data_size) {
        fprintf(stderr, "Error reading pixel data\n");
        free(pixel_data);
        fclose(input);
        return 1;
    }
    fclose(input);

    // Проверяем, что изображение ч/б (R == G == B)
    uint8_t* row = pixel_data;
    for (int32_t y = 0; y < height; ++y) {
        uint8_t* pixel = row;
        for (int32_t x = 0; x < width; ++x) {
            uint8_t r = pixel[2];
            uint8_t g = pixel[1];
            uint8_t b = pixel[0];
            if (r != g || g != b) {
                fprintf(stderr, "Image is not grayscale (R=G=B required)\n");
                free(pixel_data);
                return 1;
            }
            pixel += 3;
        }
        row += row_size;
    }

    // Инвертируем пиксели
    row = pixel_data;
    for (int32_t y = 0; y < height; ++y) {
        uint8_t* pixel = row;
        for (int32_t x = 0; x < width; ++x) {
            uint8_t inv = 255 - pixel[2]; // pixel[2] = R
            pixel[0] = inv; // B
            pixel[1] = inv; // G
            pixel[2] = inv; // R
            pixel += 3;
        }
        row += row_size;
    }

    // Записываем выходной файл
    FILE* output = fopen(output_path, "wb");
    if (!output) {
        perror("Cannot create output file");
        free(pixel_data);
        return 1;
    }

    if (fwrite(&bmp_header, sizeof(BMPHeader), 1, output) != 1) {
        fprintf(stderr, "Error writing output BMP header\n");
        goto cleanup;
    }

    if (fwrite(&info_header, sizeof(BMPInfoHeader), 1, output) != 1) {
        fprintf(stderr, "Error writing output BMP info header\n");
        goto cleanup;
    }

    // Если bfOffBits > sizeof(headers), заполняем промежуток нулями (цветовая палитра отсутствует)
    uint32_t header_end = sizeof(BMPHeader) + sizeof(BMPInfoHeader);
    if (bmp_header.bfOffBits > header_end) {
        size_t padding_size = bmp_header.bfOffBits - header_end;
        uint8_t* zero_pad = calloc(1, padding_size);
        if (!zero_pad) {
            fprintf(stderr, "Memory allocation failed\n");
            goto cleanup;
        }
        fwrite(zero_pad, 1, padding_size, output);
        free(zero_pad);
    }

    if (fwrite(pixel_data, 1, pixel_data_size, output) != pixel_data_size) {
        fprintf(stderr, "Error writing pixel data\n");
        goto cleanup;
    }

    fclose(output);
    free(pixel_data);
    return 0;

cleanup:
    fclose(output);
    free(pixel_data);
    return 1;
}
