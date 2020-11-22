#include <stdio.h>
#include <ctype.h>
#include <string.h>

int toIndex(const char* square) {
    int col=toupper(square[0])-'A';
    int row=8-(square[1]-'0');
    printf("%s col=%d row=%d\n",square,col,row);
    return row*8+col;
}

int main(int argc,char* argv[]) {
    char square[3]="A1";
    printf("index=%d %s\n",toIndex(square),square);
    strcpy(square,"A8");
    printf("index=%d %s\n",toIndex(square),square);
    strcpy(square,"H1");
    printf("index=%d %s\n",toIndex(square),square);
    strcpy(square,"H8");
    printf("index=%d %s\n",toIndex(square),square);

    return 0;
}