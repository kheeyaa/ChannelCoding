#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset 함수가 선언된 헤더 파일
#include <math.h>
#include <time.h>

// Transmission Channel 에서 사용할 것
#define NUM_RV 10000
#define K 5

char ber_in[160];
char ber_err[160];
char ber_out[160];
char ber_en[160];

char binToDec(char bin[]);
char *RepetitionEn(char in[]);
char *RepetitionDe(char in[][8]);
char *ChannelError(char in[][8]);
unsigned char *DectoBin2(unsigned char dec);

int main()
{
    int file_size, data_size, height;
    char *bmp_header; // 파일 사이즈 모를 때
    char *bmp_data;   // read
    char **input;     // 20 byte 단위
    char *output;

    FILE *fpt1;
    FILE *fpt2;

    // 1. BMP file read

    // 파일 읽기
    fpt1 = fopen("lenna.bmp", "rb"); // read binary

    if (fpt1 == NULL)
        exit(1);

    // header 읽기
    bmp_header = (char *)malloc(54); // malloc 할당
    fread(bmp_header, sizeof(char), 54, fpt1);

    fseek(fpt1, 0, SEEK_END);     //파일의 끝(seek end) + 0 byte (offset)의 위치로 FPI를 옮김
    file_size = ftell(fpt1) - 54; // 파일의 처음에 대한 FPI가 가리키는 상대적 위치를 byte로 알려줌

    // data 읽기
    if (file_size % 20 != 0)
    {
        // 20Byte 를 맞추기위해 나머지는 0으로 채운다
        data_size = file_size + (20 - file_size % 20);
        bmp_data = (char *)malloc(sizeof(char) * data_size);
        memset(bmp_data, 0, sizeof(data_size));
    }
    else
    {
        data_size = file_size;
        bmp_data = (char *)malloc(sizeof(char) * file_size); // malloc 할당
    }
    fseek(fpt1, 54, SEEK_SET);
    fread(bmp_data, sizeof(char), file_size, fpt1);
    fclose(fpt1);

    // 2. segmentation
    height = (int)(data_size / 20);
    input = (char **)malloc(sizeof(char *) * height);
    output = (char *)malloc(sizeof(char) * data_size); // desegment 저장할 곳
    for (int i = 0; i < height; i++)
    {
        input[i] = (char *)malloc(sizeof(char) * 20);
    }

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            input[i][j] = bmp_data[i * 20 + j];
        }
    }

    int N = 0;    // 패킷의 개수
    int err1 = 0; // 전체 비트에서 일어난 비트 에러 개수
    int err2 = 0;
    for (int i = 0; i < height; i++)
    {
        // 3. repetition encoder
        char(*rep_en)[8] = (void *)RepetitionEn(input[i]);

        // 4. channel error
        char(*rep_err)[8] = (void *)ChannelError(rep_en);
        for (int j = 0; j < 160; j++)
        {
            unsigned char *tp1 = DectoBin2(ber_en[j]);
            unsigned char *tp2 = DectoBin2(ber_err[j]);
            for (int k = 0; k < 3; k++)
            {
                if (tp1[k] != tp2[k])
                    err1 += 1;
            }
        }

        // 5. repetition decoder
        char *rep_de = RepetitionDe(rep_err);

        // // BER check
        N += 1; // frame의 갯수
        for (int j = 0; j < 160; j++)
        {
            if (ber_in[j] != ber_out[j])
                err2 += 1;
        }

        // 6. de-segmentation
        for (int j = 0; j < 20; j++)
        {
            output[i * 20 + j] = rep_de[j];
        }
    }
    // BER
    printf("K=%d\n%d frame: %dbit / BER1 = %.6lf BER2 = %.6lf\n", K, N, N * 20 * 8, (double)err1 / (N * 20 * 8 * 3), (double)err2 / (N * 20 * 8));

    // 7. BMP file write
    fpt2 = fopen("lenna1.bmp", "wb");              // 쓰기로 열어서
    fwrite(bmp_header, sizeof(char), 54, fpt2);    // header 작성
    fseek(fpt2, 54, SEEK_SET);                     // FPI 이동
    fwrite(output, sizeof(char), file_size, fpt2); // data 작성
    fclose(fpt2);

    for (int i = 0; i < height; i++)
    {
        free(input[i]);
    }
    free(input);
    free(output);
    free(bmp_data);
    free(bmp_header);

    return 0;
}
char binToDec(char bin[])
{ // 2진수를 10진수로
    char dec = 0;
    for (int i = 7; i >= 0; i--)
    {
        if (bin[i] != 0)
            dec += pow(2, i);
    }
    return dec;
}
char *RepetitionEn(char in[])
{
    static char out[20][8];

    for (int i = 0; i < 20; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            if (((in[i] >> j) & 0x01) == 0x00)
            {
                out[i][j] = 0;
                ber_in[i * 8 + j] = 0;
                ber_en[i * 8 + j] = 0;
            }
            else
            {
                out[i][j] = 7;
                ber_in[i * 8 + j] = 1;
                ber_en[i * 8 + j] = 7;
            }
        }
    }
    return (char *)out;
}
char *RepetitionDe(char in[][8])
{
    static char out[20] = {
        0,
    };
    char tp[8] = {
        0,
    };
    char cnt = 0;

    for (int i = 0; i < 20; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            cnt = 0;
            for (int k = 2; k >= 0; k--)
            {
                if (((in[i][j] >> k) & 0x01) == 0x01) // 1이면 카운트
                    cnt += 1;
            }
            if (cnt >= 2)
                tp[j] = 1;
            else
                tp[j] = 0;
            ber_out[i * 8 + j] = tp[j];
        }
        // tp를 10진수로 바꿔서 out에 저장
        out[i] = binToDec(tp);
    }

    return out;
}
char *ChannelError(char in[][8])
{
    static char out[20][8] = {
        0,
    };
    double random;
    char temp;
    srand(time(NULL));
    double p = 0.5 * pow(0.6, K); // error 확률

    for (int i = 0; i < 20; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            temp = 0;
            for (int k = 2; k >= 0; k--)
            {
                random = rand() % NUM_RV + 1; // 1~NUM_RV 사이의 숫자
                if (random < p * 10000)
                { // error 일어나야 함.
                    if (in[i][j] != 7)
                        temp += pow(2, k); // 0인 경우 1로
                }
                else if (in[i][j] == 7)
                    temp += pow(2, k); // 1 인경우 자리수 더함
            }
            out[i][j] = temp;
            ber_err[i * 8 + j] = temp;
        }
    }
    return (char *)out;
}
unsigned char *DectoBin2(unsigned char dec)
{ /************    10진수를 2진수로(3bit)    ************/
    unsigned char *bin = (unsigned char *)malloc(3);
    memset(bin, 0, sizeof(char) * 3);
    for (int i = 2; i >= 0; i--)
    {
        bin[i] = dec % 2; // 나머지
        dec = dec / 2;    // 몫
    }
    return bin;
}