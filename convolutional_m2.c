#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// encoder 에서 사용할 것
#define INIT 0
#define RUNNING 1
// Transmission Channel 에서 사용할 것
#define K 1
#define NUM_RV 10000

#define INPUT 0
#define OUTPUT 1

unsigned char *decToBin(unsigned char dec[]);
unsigned char encoder(int mode, int seed);
unsigned char *channelError(unsigned char in[]);
unsigned char *decToBin3bit(unsigned char dec);
char findLength(char dec_en, char state);
unsigned char binToDec(unsigned char bin[]);
unsigned char *decoder(unsigned char dec_en[]);

char ber_in[160];
char ber_out[160];
char ber_en[160];
char ber_err[160];

/*
    ************* encoding 과정 트렐리스 다이어그램(인접 행렬)으로 기록 *************
    trellisDiagramBeforeToAfter[before][after] = [input, output]
    trellisDiagramAfterToBefore[after][before] = [input, output]
*/
char trellisDiagramBeforeToAfter[4][4][2] = {
    { {0, 0}, {-1, -1}, {1, 7}, {-1, -1} },
    { {0, 7}, {-1, -1}, {1, 0}, {-1, -1} },
    { {-1, -1}, {0, 3}, {-1, -1}, {1, 4} },
    { {-1, -1}, {0, 4} , {-1, -1}, {1, 1} }
}; 
char visitedBeforeToAfter[4][4] = {
    {1, 0, 1, 0},
    {1, 0, 1, 0},
    {0, 1, 0, 1},
    {0, 1, 0, 1}
};

char trellisDiagramAfterToBefore[4][4][2] = {
    { {0, 0}, {0, 7}, {-1, -1}, {-1, -1} },
    { {-1, -1}, {-1, -1}, {0, 3}, {0, 4} }, 
    { {1, 7}, {1, 0}, {-1, -1}, {-1, -1} }, 
    { {-1, -1}, {-1, -1}, {1, 4}, {1, 3} }
}; 
char visitedAfterToBefore[4][4] = {
    {1, 1, 0, 0},
    {0, 0, 1, 1},
    {1, 1, 0, 0},
    {0, 0, 1, 1}
};


int main()
{
    int file_size, data_size, height;
    char *bmp_header;      // 파일 사이즈 모를 때
    char *bmp_data;        // read
    unsigned char **input; // 20 byte 단위
    unsigned char *output;
    unsigned char en_out[160]; // encoding 결과 저장

    FILE *fpt1;
    FILE *fpt2;

    /************     1. BMP file read     ************/

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

    /************     2. segmentation     ************/
    height = (int)(data_size / 20);
    input = (unsigned char **)malloc(sizeof(char *) * height);
    output = (unsigned char *)malloc(sizeof(char) * data_size); // desegment 저장할 곳
    for (int i = 0; i < height; i++)
    {
        input[i] = (unsigned char *)malloc(sizeof(char) * 20);
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
        /************     3. convolutional encoder     ************/
        // 3-1. segmentation된 데이터를 20프레임(20 * 8 = 160bit) 단위로 처리
        unsigned char *en_in = decToBin(input[i]); // 160bit

        // BER 확인을 위해 전역변수에 비트 저장
        for (int j = 0; j < 160; j++)
        {
            ber_in[j] = en_in[j];
        }

        // 3-2. encoding
        en_out[0] = encoder(INIT, en_in[0]);
        ber_en[0] = en_out[0];
        for (int j = 1; j < 160; j++)
        {
            en_out[j] = encoder(RUNNING, en_in[j]);
            ber_en[j] = en_out[j];
        }

        /************     4. channel error     ************/
        unsigned char *en_err = channelError(en_out);
        // BER1 chek
        for (int j = 0; j < 160; j++)
        {
            unsigned char *tp1 = decToBin3bit(ber_en[j]);
            unsigned char *tp2 = decToBin3bit(ber_err[j]);
            for (int k = 0; k < 3; k++)
            {
                if (tp1[k] != tp2[k])
                    err1 += 1;
            }
        }

        /************     5. viterbi decoder     ************/
        unsigned char *de_out = decoder(en_err);

        /************     6. de-segmentation     ************/
        for (int j = 0; j < 20; j++)
        {
            output[i * 20 + j] = de_out[j];
        }

        // BER2 check
        N += 1; // frame의 갯수
        for (int j = 0; j < 160; j++)
        {
            if (ber_in[j] != ber_out[j])
                err2 += 1;
        }
    }

    // BER
    printf("K=%d\n%d frame: %dbit BER1 = %.6lf BER2 = %.6lf\n", K, N, N * 20 * 8, (double)err1 / (N * 20 * 8 * 3), (double)err2 / (N * 20 * 8));
 
    /************     7. BMP file write     ************/
    fpt2 = fopen("lenna-m2.bmp", "wb");              // 쓰기로 열어서
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

unsigned char encoder(int mode, int seed)
{ /************    convolutional encoder, input : 1bit binary, output: decimal    ************/
    unsigned char output;
    static int temp[3], S[3];


    if (mode == INIT)
    {
        S[2] = 0x0;
        S[1] = 0x0;
        S[0] = seed;
    }
    else
    {
        S[2] = S[1];
        S[1] = S[0];
        S[0] = seed;
    }
    // modulo-2
    temp[0] = (S[0] + S[2]) % 2;
    temp[1] = (S[0] + S[1] + S[2]) % 2;
    temp[2] = (S[0] + S[1] + S[2]) % 2;

    // binary -> decimal
    output = 4 * temp[0] + 2 * temp[1] + 1 * temp[2];
    return output;
}
unsigned char *decToBin(unsigned char dec[])
{ /************    10진수를 2진수로(20frame, 8bit)    ************/
    // unsigned char *bin = (unsigned char *)malloc(160);
    // memset(bin, 0, sizeof(char) * 160); // 20 frame
    static unsigned char bin[160] = {
        0,
    };
    for (int i = 0; i < 20; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            bin[i * 8 + j] = dec[i] % 2; // 나머지
            dec[i] = dec[i] / 2;         // 몫
        }
    }
    return bin;
}
unsigned char *channelError(unsigned char in[])
{ /************    채널환경 에러발생 구현    ************/
    static unsigned char out[160] = {
        0,
    };
    double random;
    char temp;
    srand(time(NULL));
    double p = 0.5 * pow(0.6, K); // error 확률

    for (int i = 0; i < 160; i++)
    {
        unsigned char *bin = decToBin3bit(in[i]);
        temp = 0;
        for (int k = 2; k >= 0; k--)
        {
            random = rand() % NUM_RV + 1; // 1~NUM_RV 사이의 숫자
            if (random < p * 10000)
            { // error 일어나야 함.
                if (bin[k] != 1)
                    temp += pow(2, 2 - k); // 0 인 경우 1로
            }
            else if (bin[k] == 1)      // error 일어나지 않는 경우
                temp += pow(2, 2 - k); // 1 인경우 자리수 더함
        }
        out[i] = temp;
        ber_err[i] = temp;
    }
    return (unsigned char *)out;
}
unsigned char *decToBin3bit(unsigned char dec)
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
char findLength(char dec_en, char output)
{ /************    비트간 거리 측정    ************/
    char cnt = 0;
    unsigned char *bin_en = decToBin3bit(dec_en);
    unsigned char *bin_out = decToBin3bit(output);

    for (int i = 0; i < 3; i++)
    {
        if (bin_en[i] != bin_out[i])
        {
            cnt += 1;
        }
    }
    return cnt;
}
unsigned char binToDec(unsigned char bin[])
{ /************    2진수를 10진수로(8bit)    ************/

    unsigned char dec = 0;
    for (int i = 0; i < 8; i++)
    {
        if (bin[i] != 0)
            dec += pow(2, 7 - i);
    }
    return dec;
}
unsigned char *decoder(unsigned char dec_en[])
{                           
    /************    convolutional decoder, input : 20frame binary, output : 20frame decimal    ************/
    char activeState[3][4] ={
        { 1, 0, 0, 0 }, // lv0
        { 1, 0, 1, 0 }, // lv1
        { 1, 1, 1, 1 }, // lv2
    };

    char pathCost[4] = {
        0, 0, 0, 0
    };

    char survivingPath[4][161] = { 0, };
    char visitedSurvivingPath[4] = { 0, };
    char decoding[160] = {
        0,
    };
    unsigned char dec_frame[20][8];
    static unsigned char de_out[20];


    // level0 -> level1, level1 -> level2
    for(char level=0; level<2; level++){
        char tempSurvivingPath[4][161] = { 0, };
        int tempPathCost[4] = { 0, };

        for(int curState=0; curState<4; curState++){
            for(int beforeState=0; beforeState<4; beforeState++){
                if(!activeState[1][beforeState]) continue;
                if(!visitedAfterToBefore[curState][beforeState]) continue;
                // 현재 상태로 오기 까지 이전 상태를 확인
                int output = trellisDiagramAfterToBefore[curState][beforeState][OUTPUT];
                int cost = pathCost[beforeState] + findLength(dec_en[level], output);
                tempPathCost[curState] = cost;

                for(int i=0; i<=level; i++){
                    // 지나온 경로(지난 state)를 복사
                    tempSurvivingPath[curState][i] = survivingPath[beforeState][i]; 
                }
                // 직전 경로 추가
                tempSurvivingPath[curState][level+1] = curState;
            }
        }
        
        // path 갱신
        for(int state=0; state<4; state++){
            pathCost[state] = tempPathCost[state];
            for(int level=0; level<161; level++){
                survivingPath[state][level] = tempSurvivingPath[state][level];
            }
        }
    }

    // level2 -> 3 ...
    for(int level=2; level<160; level++){
        int tempPathCost[4] = { 0, };
        char tempSurvivingPath[4][161] = { 0, };

        // 현재 level 에서 survivingPath 찾기
        for(int curState=0; curState<4; curState++){
            char visitedSurvivingPath[4] = { 0, };  
            
            for(int beforeState=0; beforeState<4; beforeState++){
                if(!visitedAfterToBefore[curState][beforeState]) continue;
                // 현재 상태로 오기 까지 이전 상태를 확인
                int output = trellisDiagramAfterToBefore[curState][beforeState][OUTPUT];
                int cost = pathCost[beforeState] + findLength(dec_en[level], output);

                // 비용을 비교할 다른 경로가 이미 있고, 지금 비용이 더 비싼 경우 넘어감
                if(visitedSurvivingPath[curState] && tempPathCost[curState] <= cost) continue;
                // 더 저렴한 비용으로 경로 갱신
                visitedSurvivingPath[curState] = 1;
                tempPathCost[curState] = cost;

                for(int i=0; i<=level; i++){
                    // 지나온 경로(지난 state)를 복사
                    tempSurvivingPath[curState][i] = survivingPath[beforeState][i]; 
                }
                // 직전 경로 추가
                tempSurvivingPath[curState][level+1] = curState;
            }
        }

        // path 갱신
        for(int state=0; state<4; state++){
            pathCost[state] = tempPathCost[state];
            for(int level=0; level<=160; level++){
                survivingPath[state][level] = tempSurvivingPath[state][level]; 
            }
        }
    }


    // surviving path 선택
    int smallestCost = pathCost[0];
    int lastState = 0;
    for(int state=0; state<4; state++){
        if(smallestCost > pathCost[state]){
            smallestCost = pathCost[state];
            lastState = state;
        }
    }
    
    char *finalSurvivingPath = survivingPath[lastState];
    for(int level=0; level<160; level++){
        int beforeState = finalSurvivingPath[level];
        int afterState = finalSurvivingPath[level+1];
        int input = trellisDiagramBeforeToAfter[beforeState][afterState][INPUT];
        decoding[level] = input;
    }

    /************************/
    // BER 확인을 위해 전역변수에 비트 저장
    for (int j = 0; j < 160; j++)
    {
        ber_out[j] = decoding[j];
    }

    // decoding 결과를 8비트 단위로 저장
    for (int i = 0; i < 20; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            dec_frame[i][j] = decoding[i * 8 + j];
        }
    }

    // binary -> decimal
    for (int i = 0; i < 20; i++)
    {
        de_out[i] = binToDec(dec_frame[i]);
    }

    return de_out;
}