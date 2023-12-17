#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"



void cleanFile(){
    int fptr=open("strdiff_result.txt",O_CREATE|O_RDWR);  
    int i=0;  
    while(i<80){
        write(fptr," ",1);
        i++;
    }
    close(fptr);
}
void writeInFile(char* difference){
    int fptr=open("strdiff_result.txt",O_CREATE|O_RDWR);
    write(fptr,difference,strlen(difference));
    close(fptr);
}
char* computeDifference(int maxSize,char* str1,char* str2){
    char *difference = (char *)malloc((maxSize+2) * sizeof(char));
    for (int i = 0; i < maxSize; i++) {
        if (i >= strlen(str1)) {
            difference[i] = '1';
            continue;
        } 
        else if (i >= strlen(str2)) {
            difference[i] = '0';
            continue;
        }
        if(str1[i]>=97 &&  str1[i]<=122)
            str1[i]-=32;
        if(str2[i]>=97 &&  str2[i]<=122)
            str2[i]-=32;
        if (str1[i] >= str2[i]) {
            difference[i] = '0';
        } else {
            difference[i] = '1';
        }
    }
    difference[maxSize]='\0';
    difference[maxSize+1]='\n';
    return difference;
}
void strdiff(char *str1,char *str2) {
    int maxSize = (strlen(str1) >= strlen(str2)) ? strlen(str1) : strlen(str2);
    char *difference = computeDifference(maxSize,str1,str2);
    cleanFile();
    if(maxSize<=15 && maxSize>0){
        writeInFile(difference);
    }
    return ;
}

int main(int argc, char *argv[]){
    strdiff(argv[1],argv[2]);
    exit();
}

