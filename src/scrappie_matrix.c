#include <assert.h>
#ifdef __APPLE__
        #include <Accelerate/Accelerate.h>
#else
        #include <cblas.h>
#endif
#include <err.h>
#include <math.h>
#include <string.h>
#include "scrappie_matrix.h"

scrappie_matrix make_scrappie_matrix(int nr, int nc){
        // Matrix padded so row length is multiple of 4
        int nrq = (int)ceil(nr / 4.0);
        scrappie_matrix mat = malloc(sizeof(*mat));
        mat->nr = nr;
        mat->nrq = nrq;
        mat->nc = nc;
        int status = posix_memalign((void **) &(mat->data.v), 16, nrq * nc * sizeof(__m128));
        if(0 != status){
                warnx("Error allocating memory in %s.\n", __func__);
                free(mat);
                return NULL;
        }
        memset(mat->data.v, 0, nrq * nc * sizeof(__m128));
        return mat;
}

scrappie_matrix remake_scrappie_matrix(scrappie_matrix M, int nr, int nc){
        // Could be made more efficient when there is sufficent memory already allocated
        if((NULL == M) || (M->nr != nr) || (M->nc != nc)){
                M = free_scrappie_matrix(M);
                M = make_scrappie_matrix(nr, nc);
        }
        return M;
}

void zero_scrappie_matrix(scrappie_matrix M) {
        if(NULL != M){ return; }
        memset(M->data.f, 0, M->nrq * 4 * M->nc * sizeof(float));
}


scrappie_matrix mat_from_array(const float * x, int nr, int nc){
        scrappie_matrix res = make_scrappie_matrix(nr, nc);
        for(int col=0 ; col < nc ; col++){
                memcpy(res->data.f + col * res->nrq * 4, x + col * nr, nr * sizeof(float));
        }
        return res;
}


void fprint_scrappie_matrix(FILE * fh, const char * header, const scrappie_matrix mat, int nr, int nc){
        assert(NULL != fh);
        assert(NULL != mat);
        if(nr <= 0 || nr > mat->nr){nr = mat->nr;}
        if(nc <= 0 || nc > mat->nc){nc = mat->nc;}

        if(NULL != header){
                fputs(header, fh);
                fputc('\n', fh);
        }
        for(int c=0 ; c < nc ; c++){
                const size_t offset = c * mat->nrq * 4;
                fprintf(fh, "%4d : % 6.4f", c, mat->data.f[offset]);
                for(int r=1 ; r<nr ; r++){
                        fprintf(fh, "  % 6.4f", mat->data.f[offset + r]);
                }
                fputc('\n', fh);
        }
}


scrappie_matrix free_scrappie_matrix(scrappie_matrix mat){
        if(NULL != mat){
                free(mat->data.v);
                free(mat);
        }
        return NULL;
}

scrappie_imatrix make_scrappie_imatrix(int nr, int nc){
        // Matrix padded so row length is multiple of 4
        int nrq = (int)ceil(nr / 4.0);
        scrappie_imatrix mat = malloc(sizeof(*mat));
        mat->nr = nr;
        mat->nrq = nrq;
        mat->nc = nc;
        int status = posix_memalign((void **) &(mat->data.v), 16, nrq * nc * sizeof(__m128i));
        if(0 != status){
                warnx("Error allocating memory in %s.\n", __func__);
                free(mat);
                return NULL;
        }
        memset(mat->data.v, 0, nrq * nc * sizeof(__m128));
        return mat;
}

scrappie_imatrix remake_scrappie_imatrix(scrappie_imatrix M, int nr, int nc){
        // Could be made more efficient when there is sufficent memory already allocated
        if((NULL == M) || (M->nr != nr) || (M->nc != nc)){
                M = free_scrappie_imatrix(M);
                M = make_scrappie_imatrix(nr, nc);
        }
        return M;
}

scrappie_imatrix free_scrappie_imatrix(scrappie_imatrix mat){
        if(NULL != mat){
                free(mat->data.v);
                free(mat);
        }
        return NULL;
}

void zero_scrappie_imatrix(scrappie_imatrix M) {
        if(NULL != M){ return; }
        memset(M->data.f, 0, M->nrq * 4 * M->nc * sizeof(int));
}


scrappie_matrix affine_map(const scrappie_matrix X, const scrappie_matrix W,
                 const scrappie_matrix b, scrappie_matrix C){
        /*  Affine transform C = W^t X + b
         *  X is [nr, nc]
         *  W is [nr, nk]
         *  b is [nk]
         *  C is [nk, nc] or NULL.  If NULL then C is allocated.
         */
        if(NULL == X){
                // Input NULL due to earlier failure.  Propagate
                return NULL;
        }
        assert(NULL != W);
        assert(NULL != b);
        assert(W->nr == X->nr);
        C = remake_scrappie_matrix(C, W->nc, X->nc);
        if(NULL == C){
                return NULL;
        }

        /* Copy bias */
        for( int c = 0 ; c < C->nc; c++){
                memcpy(C->data.v + c * C->nrq, b->data.v, C->nrq * sizeof(__m128));
        }

        /* Affine transform */
        cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, W->nc, X->nc, W->nr, 1.0, W->data.f, W->nrq * 4, X->data.f, X->nrq * 4, 1.0, C->data.f, C->nrq * 4);


        return C;
}

scrappie_matrix affine_map2(const scrappie_matrix Xf, const scrappie_matrix Xb,
                  const scrappie_matrix Wf, const scrappie_matrix Wb,
                  const scrappie_matrix b, scrappie_matrix C){
        if(NULL == Xf || NULL == Xb){
                // Input NULL due to earlier failure.  Propagate
                return NULL;
        }
        assert(NULL != Wf);
        assert(NULL != Wb);
        assert(NULL != b);
        assert(Wf->nr == Xf->nr);
        assert(Wb->nr == Xb->nr);
        assert(Xf->nc == Xb->nc);
        assert(Wf->nc == Wb->nc);
        C = remake_scrappie_matrix(C, Wf->nc, Xf->nc);
        if(NULL == C){
                return NULL;
        }

        /* Copy bias */
        for( int c = 0 ; c < C->nc; c++){
                memcpy(C->data.v + c * C->nrq, b->data.v, C->nrq * sizeof(__m128));
        }

        /* Affine transform -- forwards*/
        cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, Wf->nc, Xf->nc, Wf->nr, 1.0, Wf->data.f, Wf->nrq * 4, Xf->data.f, Xf->nrq * 4, 1.0, C->data.f, C->nrq * 4);
        /* Affine transform -- backwards*/
        cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, Wb->nc, Xb->nc, Wb->nr, 1.0, Wb->data.f, Wb->nrq * 4, Xb->data.f, Xb->nrq * 4, 1.0, C->data.f, C->nrq * 4);
        return C;
}


__m128 mask(int i){
        return (__m128)(__v4sf){i>=1, i>=2, i>=3, 0.0f};
}

void row_normalise_inplace(scrappie_matrix C){
        if(NULL == C){
                // Input NULL due to earlier failure.  Propagate
                return;
        }
        for(int col=0 ; col < C->nc ; col++){
                const size_t offset = col * C->nrq;
                __m128 sum = _mm_setzero_ps();
                for(int row=0 ; row < C->nrq ; row++){
                        sum += C->data.v[offset + row];
                }
                sum -= C->data.v[offset + C->nrq - 1] * mask(C->nr - C->nrq * 4);
                const __m128 psum = _mm_hadd_ps(sum, sum);
                const __m128 tsum = _mm_hadd_ps(psum, psum);

                for(int row=0 ; row < C->nrq ; row++){
                        C->data.v[offset + row] /= tsum;
                }
        }
}

float max_scrappie_matrix(const scrappie_matrix x){
        if(NULL == x){
                // Input NULL due to earlier failure.  Propagate
                return NAN;
        }
        float amax = x->data.f[0];
        for(int col=0 ; col < x->nc ; col++){
                const size_t offset = col * x->nrq * 4;
                for(int r=0 ; r < x->nr ; r++){
                        if(amax < x->data.f[offset + r]){
                                amax = x->data.f[offset + r];
                        }
                }
        }
        return amax;
}

float min_scrappie_matrix(const scrappie_matrix x){
        if(NULL == x){
                // Input NULL due to earlier failure.  Propagate
                return NAN;
        }
        float amin = x->data.f[0];
        for(int col=0 ; col < x->nc ; col++){
                const size_t offset = col * x->nrq * 4;
                for(int r=0 ; r < x->nr ; r++){
                        if(amin < x->data.f[offset + r]){
                                amin = x->data.f[offset + r];
                        }
                }
        }
        return amin;
}


int argmax_scrappie_matrix(const scrappie_matrix x){
        if(NULL == x){
                // Input NULL due to earlier failure.  Propagate
                return -1;
        }
        float amax = x->data.f[0];
        int imax = 0;

        for(int col=0 ; col < x->nc ; col++){
                const size_t offset = col * x->nrq * 4;
                for(int r=0 ; r < x->nr ; r++){
                        if(amax < x->data.f[offset + r]){
                                amax = x->data.f[offset + r];
                                imax = offset + r;
                        }
                }
        }
        return imax;
}

int argmin_scrappie_matrix(const scrappie_matrix x){
        if(NULL == x){
                // Input NULL due to earlier failure.  Propagate
                return -1;
        }
        float amin = x->data.f[0];
        int imin = 0;

        for(int col=0 ; col < x->nc ; col++){
                const size_t offset = col * x->nrq * 4;
                for(int r=0 ; r < x->nr ; r++){
                        if(amin < x->data.f[offset + r]){
                                amin = x->data.f[offset + r];
                                imin = offset + r;
                        }
                }
        }
        return imin;
}

