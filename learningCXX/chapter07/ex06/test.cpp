#include <iostream>
#include <cassert>

using namespace std;

// a C implementation of qsort
#if 0

#include <assert.h>
void qsort(void  *base,
           size_t nel,
           size_t width,
           int (*comp)(const void *, const void *))
{
	size_t wgap, i, j, k;
	char tmp;

	if ((nel > 1) && (width > 0)) {
		assert(nel <= ((size_t)(-1)) / width); /* check for overflow */
		wgap = 0;
		do {
			wgap = 3 * wgap + 1;
		} while (wgap < (nel-1)/3);
		/* From the above, we know that either wgap == 1 < nel or */
		/* ((wgap-1)/3 < (int) ((nel-1)/3) <= (nel-1)/3 ==> wgap <  nel. */
		wgap *= width;			/* So this can not overflow if wnel doesn't. */
		nel *= width;			/* Convert nel to 'wnel' */
		do {
			i = wgap;
			do {
				j = i;
				do {
					register char *a;
					register char *b;

					j -= wgap;
					a = j + ((char *)base);
					b = a + wgap;
					if ((*comp)(a, b) <= 0) {
						break;
					}
					k = width;
					do {
						tmp = *a;
						*a++ = *b;
						*b++ = tmp;
					} while (--k);
				} while (j >= wgap);
				i += width;
			} while (i < nel);
			wgap = (wgap - width)/3;
		} while (wgap);
	}
}
#endif

typedef int (*CFT)(const void *, const void *);

void 
my_qsort(void *base, size_t nel, size_t width, CFT cmp)
{
	if ((nel > 1) && (width > 0)) {
		assert(nel <= ((size_t)(-1)) / width); /* check for overflow */
		size_t wgap = 0;
		do {
			wgap = 3 * wgap + 1;
		} while (wgap < (nel-1)/3);
		/* From the above, we know that either wgap == 1 < nel or */
		/* ((wgap-1)/3 < (int) ((nel-1)/3) <= (nel-1)/3 ==> wgap <  nel. */
		wgap *= width;			/* So this can not overflow if wnel doesn't. */
		nel *= width;			/* Convert nel to 'wnel' */
		do {
			size_t i = wgap;
			do {
				size_t j = i;
				do {
					register char *a;
					register char *b;

					j -= wgap;
					a = j + ((char *)base);
					b = a + wgap;
					if ((*cmp)(a, b) <= 0) {
						break;
					}
					size_t k = width;
					do {
						char tmp = *a;
						*a++ = *b;
						*b++ = tmp;
					} while (--k);
				} while (j >= wgap);
				i += width;
			} while (i < nel);
			wgap = (wgap - width)/3;
		} while (wgap);
	}
}

int 
int_cmp(const void *a, const void *b)
{
    return (*static_cast<const int*>(a) - *static_cast<const int*>(b));
}

int
main(int argc, char *argv[])
{
    int a[10] = {1, 0, 9, 2, 3, 8, 7, 4, 5, 6};

    // output before sort
    cout << "before sort: ";
    size_t n = sizeof(a) / sizeof(int);
    for (unsigned int i = 0; i < n; i++)
        cout << a[i] << ", ";
    cout << endl;
    // sort
    my_qsort(a, n, sizeof(int), int_cmp);
    // output after sort
    cout << "after sort: ";
    for (unsigned int i = 0; i < n; i++)
        cout << a[i] << ", ";
    cout << endl;

    return 0;
}
