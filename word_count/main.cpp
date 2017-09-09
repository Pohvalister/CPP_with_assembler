
#include <iostream>


using namespace std;
#include  <xmmintrin.h> //SSE

//



uint64_t simpleCount(std::string const &str) {
    uint64_t answer = 0;
    for (int i = 1; i < str.length(); i++) {
        if (str[i] == ' ' && str[i - 1] != ' ') {
            answer++;
        }
    }
    if (str[str.length() - 1] != ' ')
        answer += 1;
    return answer;
}

uint64_t spacesCount(std::string const &str) {
    size_t symCount = str.size();
    char const *str_data = str.data();
    if (symCount < 64) {
        return simpleCount(str);
    }
    uint64_t answer = 0;

    size_t aligning = 0;
    bool bsBefore = true;
    while (((size_t) (str_data + aligning) % 16 != 0)&&aligning==0) {
        if (bsBefore && *(str_data + aligning) != ' ')
            answer++;
        bsBefore = (*(str_data + aligning) == ' ');
        aligning++;
    }
    if (str[aligning] != ' ' && bsBefore)
        answer++;
    //backspace then symbol == answer++

    //main part - comparing symbols with template
    //__m128i целое 128-битное число
    __m128i spaces_temp = _mm_set_epi8(' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    __m128i spaces_curr, spaces_shifted;
    __m128i borders;
    __m128i howMany;

    const __m128i zeroVec = _mm_setzero_si128();
    size_t i = aligning;
    __m128i result = _mm_setzero_si128();
    for (; i < symCount - 16; i += 16) {
      
        spaces_curr = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str_data + i)), spaces_temp);//где правда -> -1 (0xff)
        spaces_shifted = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *) (str_data + i + 1)), spaces_temp);


        borders = _mm_andnot_si128(spaces_shifted,
                                   spaces_curr);//Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the 128-bit value in a.

        result = _mm_sub_epi8(result, borders);//-где правда -> +1
        if (_mm_movemask_epi8(result) != 0) {//int от едениц (ai[7]<<i) - когда заполнится
            howMany = _mm_sad_epu8(zeroVec, result);//r0=sum && r4 = sum
            answer += (((uint64_t *) (&howMany))[0] + ((uint64_t *) (&howMany))[1]);
            result = _mm_setzero_si128();
        }
    }
    //remaining
    howMany = _mm_sad_epu8(zeroVec, result);
    answer += (((uint64_t *) (&howMany))[0] + ((uint64_t *) (&howMany))[1]);
    bsBefore = str[i] == ' ';

    while (i != symCount) {
        if (bsBefore && (str[i] != ' '))
            answer++;
        bsBefore = ((str[i]) == ' ');
        i++;
    }
    return answer;
}

int main() {
    string str = "";
    for (int i = 1001; i > 100; i--) {
        for (int j = 0; j < i; j++)
            str += "a";
        str+=" b";
        str += ' ';
    }

    std::cout << spacesCount(str) << '\n';

    str = " a";
    for (int i = 0; i < 100; i++) {
        str += ' ';
    }

    std::cout << spacesCount(str) << '\n';
}
