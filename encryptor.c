#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

#define MAX_WORD_LENGTH 1000
#define MAX_NUMBER_OF_WORDS 10009

bool fileExists (char *filename) {
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

char* nextWord(FILE *inputFile) {
    char* word = malloc(MAX_WORD_LENGTH * sizeof(char));
    if(fscanf(inputFile, "%s", word) == 0) {
        word = "\0";
        return word;
    }
    return word;
}

void decrypt(FILE *inputFile, FILE *outputFile, FILE *keyFile) {
    printf("DECRYPTING\n");
    bool endReached = false;
    int pos = 0, len, forks = 0;
    int numberOfWords = 0;

    char wordsShmName[] = "words";
    int wordsShmFd = shm_open(wordsShmName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR );
    if(wordsShmFd < 0) {
        perror(NULL);
        exit(errno);
    }
    
    char keysShmName[] = "keys";
    int keysShmFd = shm_open(keysShmName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR );
    if(keysShmFd < 0) {
        perror(NULL);
        exit(errno);
    }

   char *words[MAX_NUMBER_OF_WORDS];

   

    while(endReached == false) {
        char* word = nextWord(inputFile);
        
        if(strcmp(word, "\0") == 0) {
            endReached = true;
            continue;
        }

        words[numberOfWords] = word;
        numberOfWords ++;
    }

    
    size_t pg_size = getpagesize();
    size_t shm_size_child = (MAX_WORD_LENGTH / pg_size + 1) * pg_size;
    size_t shm_size = shm_size_child * numberOfWords;

    if ( ftruncate ( wordsShmFd , shm_size ) == -1) {
        perror ( NULL );
        shm_unlink ( wordsShmName );
        exit(errno);
    }

    if ( ftruncate ( keysShmFd , shm_size * sizeof(int) ) == -1) {
        perror ( NULL );
        shm_unlink ( keysShmName );
        exit(errno);
    }

    for(int i = 0; i < numberOfWords; i ++) {
        char *words_ptr = mmap (0 , shm_size_child , PROT_WRITE , MAP_SHARED , wordsShmFd , i*shm_size_child);
    
        if ( words_ptr == MAP_FAILED ) {
            printf("MAP FAILED");
            perror ( NULL );
            shm_unlink ( wordsShmName );
            exit(errno);
        }
        int *keys_ptr = mmap (0 , sizeof(int)*shm_size_child , PROT_WRITE , MAP_SHARED , keysShmFd , sizeof(int)*i*shm_size_child);
    
        if ( keys_ptr == MAP_FAILED ) {
            printf("MAP FAILED");
            perror ( NULL );
            shm_unlink ( keysShmName );
            exit(errno);
        }

        memcpy(words_ptr, words[i], strlen(words[i]) + 1);
        for(int j = 0; j < strlen(words[i]); j ++) {
            fscanf(keyFile, "%d", &keys_ptr[j]);
        }

        int cpid = fork();
        if(cpid == 0) {
            char *words_ptr = mmap (0 , shm_size_child , PROT_WRITE , MAP_SHARED , wordsShmFd , i*shm_size_child);
            int *keys_ptr = mmap (0 , sizeof(int) * shm_size_child , PROT_READ , MAP_SHARED , keysShmFd , sizeof(int)*i*shm_size_child);
            

            if ( words_ptr == MAP_FAILED ) {
                printf("MAP FAILED");
                perror ( NULL );
                shm_unlink ( wordsShmName );
                exit(errno);
            }

            if ( keys_ptr == MAP_FAILED ) {
                printf("MAP FAILED");
                perror ( NULL );
                shm_unlink ( keysShmName );
                exit(errno);
            }

            int len = strlen(words_ptr);
            char* word = malloc(len);
            memcpy(word, words_ptr, len);

            
            for(int j = 0; j < len; j ++) {
                words_ptr[keys_ptr[j]] = word[j];
            }
            
            exit(0);
        }
    }

    for(int i = 0; i < numberOfWords; i ++) {
        wait(NULL);
    }

    char *words_ptr = mmap (0 , shm_size , PROT_READ , MAP_SHARED , wordsShmFd , 0);
    if ( words_ptr == MAP_FAILED ) {
        printf("MAP FAILED");
        perror ( NULL );
        shm_unlink ( wordsShmName );
        exit(errno);
    }

    for(int i = 0; i < numberOfWords; i ++) {
        fprintf(outputFile, "%s ", (words_ptr + i * shm_size_child));
    }
}

int* randomPermutation(int n) {
    srand(getpid());
    int* r = malloc(n * sizeof(int));
    for(int i=0;i<n;++i){
        r[i]=i;
    }
    for(int i=0; i<n; ++i){
        int randIdx = rand() % n;
        int t = r[i];
        r[i] = r[randIdx];
        r[randIdx] = t;
    }
    return r;
}

void encrypt(FILE *inputFile, FILE *outputFile, FILE *keyFile) {
    printf("ENCRYPTING\n");
    bool endReached = false;
    int pos = 0, len, forks = 0;
    int numberOfWords = 0;

    char wordsShmName[] = "words";
    int wordsShmFd = shm_open(wordsShmName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR );
    if(wordsShmFd < 0) {
        perror(NULL);
        exit(errno);
    }
    
    char keysShmName[] = "keys";
    int keysShmFd = shm_open(keysShmName, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR );
    if(keysShmFd < 0) {
        perror(NULL);
        exit(errno);
    }

   char *words[MAX_NUMBER_OF_WORDS];

    while(endReached == false) {
        char* word = nextWord(inputFile);
        
        if(strcmp(word, "\0") == 0) {
            endReached = true;
            break;
        }

        words[numberOfWords] = word;
        numberOfWords ++;
    }

    
    size_t pg_size = getpagesize();
    size_t shm_size_child = (MAX_WORD_LENGTH / pg_size + 1) * pg_size;
    size_t shm_size = shm_size_child * numberOfWords;

    if ( ftruncate ( wordsShmFd , shm_size ) == -1) {
        perror ( NULL );
        shm_unlink ( wordsShmName );
        exit(errno);
    }

    if ( ftruncate ( keysShmFd , shm_size * sizeof(int) ) == -1) {
        perror ( NULL );
        shm_unlink ( keysShmName );
        exit(errno);
    }
    for(int i = 0; i < numberOfWords; i ++) {
        char *words_ptr = mmap (0 , shm_size_child , PROT_WRITE , MAP_SHARED , wordsShmFd , i*shm_size_child);
    
        if ( words_ptr == MAP_FAILED ) {
            printf("MAP FAILED");
            perror ( NULL );
            shm_unlink ( wordsShmName );
            exit(errno);
        }

        memcpy(words_ptr, words[i], strlen(words[i]) + 1);
         // printf("%s %s\n", words[i], words_ptr);

        int cpid = fork();
        if(cpid == 0) {
            char *words_ptr = mmap (0 , shm_size_child , PROT_WRITE , MAP_SHARED , wordsShmFd , i*shm_size_child);
            int *keys_ptr = mmap (0 , sizeof(int) * shm_size_child , PROT_WRITE , MAP_SHARED , keysShmFd , sizeof(int)*i*shm_size_child);
            // printf("%s ", words_ptr);
            if ( words_ptr == MAP_FAILED ) {
                printf("MAP FAILED");
                perror ( NULL );
                shm_unlink ( wordsShmName );
                exit(errno);
            }

            if ( keys_ptr == MAP_FAILED ) {
                printf("MAP FAILED");
                perror ( NULL );
                shm_unlink ( keysShmName );
                exit(errno);
            }

            int len = strlen(words_ptr);
            char* word = malloc(len);
            memcpy(word, words_ptr, len);

            
            memcpy(keys_ptr, randomPermutation(len), sizeof(int) * len);
            for(int j = 0; j < len; j ++) {
                words_ptr[j] = word[keys_ptr[j]];
            }
            
            exit(0);
        }
    }


    for(int i = 0; i < numberOfWords; i ++) {
        wait(NULL);
    }

    char *words_ptr = mmap (0 , shm_size , PROT_READ , MAP_SHARED , wordsShmFd , 0);
    if ( words_ptr == MAP_FAILED ) {
        printf("MAP FAILED");
        perror ( NULL );
        shm_unlink ( wordsShmName );
        exit(errno);
    }

    int *keys_ptr = mmap (0 , sizeof(int) * shm_size , PROT_WRITE , MAP_SHARED , keysShmFd , 0);
    if ( keys_ptr == MAP_FAILED ) {
        printf("MAP FAILED");
        perror ( NULL );
        shm_unlink ( keysShmName );
        exit(errno);
    }

    for(int i = 0; i < numberOfWords; i ++) {
        fprintf(outputFile, "%s ", (words_ptr + i * shm_size_child));

        for(int j = 0; j < strlen( words_ptr + i * shm_size_child); j ++)
            fprintf(keyFile, "%d ", keys_ptr[i * shm_size_child + j]);
        // fprintf(keyFile, "\n");
    }
}

int main(int argc, char** argv) {

    char* inputFilename = argv[1];
    FILE *inputFile = fopen(argv[1], "r");

    if(fileExists(inputFilename) == 0) {
        printf("%s %s\n", inputFilename, "does not exist.");
        return 0;
    }
    bool decryptFlag = false;

    FILE *keyFile;
    if(argc == 2) {
        keyFile = fopen("keys.txt", "w");
    }
    else {
        keyFile = fopen(argv[2], "r");
        decryptFlag = true;
    }

    FILE *outputFile = fopen("output.txt", "w");
    



    if(decryptFlag == false) {
        encrypt(inputFile, outputFile, keyFile);
    }
    else {
        decrypt(inputFile, outputFile, keyFile);
    }






    return 0;
}