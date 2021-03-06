#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_STRING 100


const int vocab_hash_size = 30000000;  // Maximum 30 * 0.7 = 21M words in the vocabulary


struct vocab_word {
  long long cn;
  int *point;
  char *word, *code, codelen;
};
struct vocab_word *vocab;
float *syn0;
long long vocab_size, layer1_size, vocab_count; 
int *vocab_hash;
char syn0_file[MAX_STRING], input_file[MAX_STRING], output_file[MAX_STRING];

void ReadWord(char *word, FILE *fin) {
    int a = 0, ch;
    while (!feof(fin)) {
        ch = fgetc(fin);
        if (ch == 13) continue;
        if ((ch == ' ') || (ch == '\t') || (ch == '\n')) {
            if (a > 0) {
                if (ch == '\n') ungetc(ch, fin);
                break;
            }
            if (ch == '\n') {
                strcpy(word, (char *)"</s>");
                return;
            } else continue;
        }
        word[a] = ch;
        a++;
        if (a >= MAX_STRING - 1) a--;   // Truncate too long words
    }
    word[a] = 0;
}

int GetWordHash(char *word) {
    unsigned long long a, hash = 0;
    for (a = 0; a < strlen(word); a++) hash = hash * 257 + word[a];
    hash = hash % vocab_hash_size;
    return hash;
}

int SearchVocab(char *word) {
    unsigned int hash = GetWordHash(word);
    while (1) {
        if (vocab_hash[hash] == -1) return -1;
        if (!strcmp(word, vocab[vocab_hash[hash]].word)) return vocab_hash[hash];
        hash = (hash + 1) % vocab_hash_size;
    }
    return -1;
}

int AddWordToVocab(char *word) {
    unsigned int hash, length = strlen(word) + 1;
    if (length > MAX_STRING) length = MAX_STRING;
    vocab[ vocab_count ].word = (char *)calloc(length, sizeof(char));
    strcpy(vocab[ vocab_count ].word, word);
    vocab_count++;
    hash = GetWordHash(word);
    while (vocab_hash[hash] != -1) hash = (hash + 1) % vocab_hash_size;
    vocab_hash[hash] = vocab_count - 1;
    return vocab_count - 1;
}

int Init(){
    long long a = 0, b = 0;
    char word[MAX_STRING];
    FILE* fin;
    
    //syn0
    fin = fopen(syn0_file, "rb");
    if (fin == NULL) {
        printf("ERROR: word vector file not found!\n");
        exit(1);
    }
    fseek(fin, 0, SEEK_SET);
    ReadWord(word, fin);
    vocab_size = atoll(word);
        
    ReadWord(word, fin);
    layer1_size = atoll( word );
    
    vocab = (struct vocab_word *)calloc(vocab_size, sizeof(struct vocab_word));

    a = posix_memalign((void **)&syn0, 128, (long long)vocab_size * layer1_size * sizeof(float));
    if (syn0 == NULL) {printf("Memory allocation failed\n"); exit(1);}
    
    
 
    while(!feof(fin)){
        ReadWord(word, fin);
        if (!strcmp(word, (char *)"</s>")){
            if (feof(fin)) break;
            ReadWord(word, fin);
            a = AddWordToVocab(word);
            b = 0;
            continue;
        }
        syn0 [ a * layer1_size + b ] = atof(word);
        b ++;
    }
    
    fclose(fin);
    return 0;
}

void SVMFormat(){
    long long c, i;
    int word_cnt;
    FILE *fin, *fo;
    char word[MAX_STRING], label[MAX_STRING];
    fin = fopen(input_file,"rb");
    fo = fopen(output_file,"wb");
    float *feature_vector = (float *)malloc(layer1_size * sizeof(int));
    int unknown_index = SearchVocab("unknown_word");

    while(!feof(fin)) {
        ReadWord(word,fin); // user
        ReadWord(label,fin);
        word_cnt = 0;
        for (c = 0; c < layer1_size; c++) feature_vector[c] = 0;
        ReadWord(word,fin);
        while(strcmp(word,(char*)"</s>") && !feof(fin)){ // not equal
            i = SearchVocab(word);
            if (i == -1) i = unknown_index;
            for (c = 0; c < layer1_size; c++) {
                feature_vector[c] += syn0[ i * layer1_size + c];
            }
            word_cnt ++;
            ReadWord(word,fin);
        }
        if (word_cnt > 0) {
            fprintf(fo, "%s ", label);
            for (c = 0; c < layer1_size; c++) fprintf(fo, "%d:%.10f ", (int)c+1, feature_vector[c]/word_cnt);
            fprintf(fo, "\n");          
        }
    }
    fclose(fin);
    fclose(fo);
}

int ArgPos(char *str, int argc, char **argv) {
    int a;
    for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
        if (a == argc - 1) {
            printf("Argument missing for %s\n", str);
            exit(1);
        }
        return a;
    }
    return -1;
}

int main(int argc, char **argv) {
    int i;
    long long a;
    if (argc == 1) {
        printf("Usage: %s -word-vec <word_vector_file> -input <input_file> with format <id:rating:reveiw> -output <output_file> \n", argv[0]);
    }
    if ((i = ArgPos((char *)"-word-vec", argc, argv)) > 0) strcpy(syn0_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-input", argc, argv)) > 0) strcpy(input_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-output", argc, argv)) > 0) strcpy(output_file, argv[i + 1]);
    
    vocab_hash = (int *)calloc(vocab_hash_size, sizeof(int));
    for (a = 0; a < vocab_hash_size; a++) vocab_hash[a] = -1;
    Init();
    SVMFormat();
    return 0;
}

