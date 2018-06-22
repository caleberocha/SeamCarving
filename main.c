#include <stdio.h>
#include <stdlib.h>
#include <string.h>		// Para usar strings
#include <limits.h>

#ifdef WIN32
#include <windows.h>    // Apenas para Windows
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>     // Funções da OpenGL
#include <GL/glu.h>    // Funções da GLU
#include <GL/glut.h>   // Funções da FreeGLUT
#endif

// SOIL é a biblioteca para leitura das imagens
#include "SOIL.h"

// Um pixel RGB (24 bits)
typedef struct {
    unsigned char r, g, b;
} RGB;

// Uma imagem RGB
typedef struct {
    int width, height;
    RGB* img;
} Img;

// Energia acumulada, contem referência para o pixel anterior que faz parte da soma
typedef struct {
    long energy;
    int edgeTo;
} AccEnergyPixel;

// Protótipos
void load(char* name, Img* pic);
void uploadTexture();

// Funções da interface gráfica e OpenGL
void init();
void draw();
void keyboard(unsigned char key, int x, int y);

// Energy
void computeEnergy(Img img, Img mask, long* e);
void computeSeam(Img img, long* energy, int* seam);
void removeSeam(Img* img, int* seam);
void imageCopy(Img in, Img *out);
void crop(Img* img, Img mask, int pixels);

// Largura e altura da janela
int width, height;

// Identificadores de textura
GLuint tex[3];

// As 3 imagens
Img pic[3];

// Imagem selecionada (0,1,2)
int sel;

// Carrega uma imagem para a struct Img
void load(char* name, Img* pic)
{
    int chan;
    pic->img = (RGB*) SOIL_load_image(name, &pic->width, &pic->height, &chan, SOIL_LOAD_RGB);
    if(!pic->img)
    {
        printf( "SOIL loading error: '%s'\n", SOIL_last_result() );
        exit(1);
    }
    printf("Load: %d x %d x %d\n", pic->width, pic->height, chan);
}

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("seamcarving [origem] [mascara]\n");
        printf("Origem é a imagem original, mascara é a máscara desejada\n");
        exit(1);
    }
	glutInit(&argc,argv);

	// Define do modo de operacao da GLUT
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

	// pic[0] -> imagem original
	// pic[1] -> máscara desejada
	// pic[2] -> resultado do algoritmo

	// Carrega as duas imagens
    load(argv[1], &pic[0]);
    load(argv[2], &pic[1]);

    if(pic[0].width != pic[1].width || pic[0].height != pic[1].height) {
        printf("Imagem e máscara com dimensões diferentes!\n");
        exit(1);
    }

    // A largura e altura da janela são calculadas de acordo com a maior
    // dimensão de cada imagem
    width = pic[0].width;
    height = pic[0].height;

    // A largura e altura da imagem de saída são iguais às da imagem original (1)
    pic[2].width  = pic[1].width;
    pic[2].height = pic[1].height;

	// Especifica o tamanho inicial em pixels da janela GLUT
	glutInitWindowSize(width, height);

	// Cria a janela passando como argumento o titulo da mesma
	glutCreateWindow("Seam Carving");

	// Registra a funcao callback de redesenho da janela de visualizacao
	glutDisplayFunc(draw);

	// Registra a funcao callback para tratamento das teclas ASCII
	glutKeyboardFunc (keyboard);

	// Cria texturas em memória a partir dos pixels das imagens
    tex[0] = SOIL_create_OGL_texture((unsigned char*) pic[0].img, pic[0].width, pic[0].height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);
    tex[1] = SOIL_create_OGL_texture((unsigned char*) pic[1].img, pic[1].width, pic[1].height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);

    // Exibe as dimensões na tela, para conferência
    printf("Origem  : %s %d x %d\n", argv[1], pic[0].width, pic[0].height);
    printf("Destino : %s %d x %d\n", argv[2], pic[1].width, pic[0].height);
    sel = 0; // pic1

	// Define a janela de visualizacao 2D
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(0.0,width,height,0.0);
	glMatrixMode(GL_MODELVIEW);

    // Aloca memória para a imagem de saída
	pic[2].img = malloc(pic[1].width * pic[1].height * 3); // W x H x 3 bytes (RGB)
	// Pinta a imagem resultante de preto!
	memset(pic[2].img, 0, width*height*3);

    // Cria textura para a imagem de saída
	tex[2] = SOIL_create_OGL_texture((unsigned char*) pic[2].img, pic[2].width, pic[2].height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);

	// Entra no loop de eventos, não retorna
	imageCopy(pic[0], &pic[2]);
    glutMainLoop();
}


// Gerencia eventos de teclado
void keyboard(unsigned char key, int x, int y)
{
    if(key==27) {
      // ESC: libera memória e finaliza
      free(pic[0].img);
      free(pic[1].img);
      free(pic[2].img);
      exit(1);
    }
    if(key >= '1' && key <= '3')
        // 1-3: seleciona a imagem correspondente (origem, máscara e resultado)
        sel = key - '1';
    if(key == 's') {
        //imageCopy(pic[0], &pic[2]);
        crop(&pic[2], pic[1], 4);
        printf("Dimensões: %d x %d\n", pic[2].width, pic[2].height);
        sel = 2;

        uploadTexture();
    }
    glutPostRedisplay();
}

// Faz upload da imagem para a textura,
// de forma a exibi-la na tela
void uploadTexture()
{
    //tex[2] = SOIL_create_OGL_texture((unsigned char*) pic[2].img, pic[2].width, pic[2].height, SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
        pic[2].width, pic[2].height, 0,
        GL_RGB, GL_UNSIGNED_BYTE, pic[2].img);
    glDisable(GL_TEXTURE_2D);
}

// Callback de redesenho da tela
void draw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Preto
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // Para outras cores, veja exemplos em /etc/X11/rgb.txt

    glColor3ub(255, 255, 255);  // branco

    // Ativa a textura corresponde à imagem desejada
    glBindTexture(GL_TEXTURE_2D, tex[sel]);
    // E desenha um retângulo que ocupa toda a tela
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);

    glTexCoord2f(0,0);
    glVertex2f(0,0);

    glTexCoord2f(1,0);
    glVertex2f(pic[sel].width,0);

    glTexCoord2f(1,1);
    glVertex2f(pic[sel].width, pic[sel].height);

    glTexCoord2f(0,1);
    glVertex2f(0,pic[sel].height);

    glEnd();
    glDisable(GL_TEXTURE_2D);

    // Exibe a imagem
    glutSwapBuffers();
}

void computeEnergy(Img img, Img mask, long* e)
{
    int x, y, xr, xg, xb, yr, yg, yb;

    for(int i = 0; i < img.height * img.width; i++) {
        int m = mask.img[i].r - mask.img[i].g;
        if(m > 200) {
            e[i] = -999999999;
        } else if(m < -200) {
            e[i] = 999999999;
        } else {
            x = i % img.width;
            y = i / img.width;

            if(x == 0) {
                xr = img.img[i+1].r - img.img[y*(img.width-1)].r;
                xg = img.img[i+1].g - img.img[y*(img.width-1)].g;
                xb = img.img[i+1].b - img.img[y*(img.width-1)].b;
            } else if(x == img.width-1 || img.img[i+1].r < 0) {
                xr = img.img[y*img.width].r - img.img[i-1].r;
                xg = img.img[y*img.width].g - img.img[i-1].g;
                xb = img.img[y*img.width].b - img.img[i-1].b;
            } else {
                xr = img.img[i+1].r - img.img[i-1].r;
                xg = img.img[i+1].g - img.img[i-1].g;
                xb = img.img[i+1].b - img.img[i-1].b;
            }

            if(y == 0) {
                yr = img.img[i+img.width].r - img.img[x+img.width*(img.height-1)].r;
                yg = img.img[i+img.width].g - img.img[x+img.width*(img.height-1)].g;
                yb = img.img[i+img.width].b - img.img[x+img.width*(img.height-1)].b;
            } else if(y == img.height-1) {
                yr = img.img[x].r - img.img[i-img.width].r;
                yg = img.img[x].g - img.img[i-img.width].g;
                yb = img.img[x].b - img.img[i-img.width].b;
            } else {
                yr = img.img[i+img.width].r - img.img[i-img.width].r;
                yg = img.img[i+img.width].g - img.img[i-img.width].g;
                yb = img.img[i+img.width].b - img.img[i-img.width].b;
            }
            e[i] = xr*xr + xg*xg + xb*xb + yr*yr + yg*yg + yb*yb;
        }
    }
}

void computeSeam(Img img, long* energy, int* seam)
{
    // Acumulação de energias
    AccEnergyPixel accEnergy[img.width * img.height];

    for(int i = 0; i < img.width; i++) {
        accEnergy[i].energy = energy[i];
        accEnergy[i].edgeTo = -1;
    }
    for(int i = img.width; i < img.height*img.width; i++) {
        int x = i % img.width;

        int idx = i-img.width;
        long e = accEnergy[idx].energy;
        if(x == 0) {
            if(accEnergy[idx+1].energy < e) {
                e = accEnergy[idx+1].energy;
                idx++;
            }
        } else if(x == img.width-1) {
            if(accEnergy[idx-1].energy < e) {
                e = accEnergy[idx-1].energy;
                idx--;
            }
        } else {
            if(accEnergy[idx+1].energy < e) {
                e = accEnergy[idx+1].energy;
                idx++;
            }
            if(accEnergy[idx-2].energy < e) {
                e = accEnergy[idx-2].energy;
                idx -= 2;
            }
        }
        accEnergy[i].energy += e;
        accEnergy[i].edgeTo = idx;
    }

    // Obtém o menor valor acumulado
    int index = img.width * (img.height-1);
    long sum = accEnergy[index].energy;
    for(int i = index; i < img.width*img.height; i++) {
        if(sum > accEnergy[i].energy) {
            sum = accEnergy[i].energy;
            index = i;
        }
    }
    //printf("Menor: %d (%d), %lu\n", index, index % img.width, accEnergy[index].energy);

    seam[0] = index;
    int previousIndex = accEnergy[index].edgeTo;
    int i = 1;
    while(previousIndex != -1) {
        seam[i] = previousIndex;
        previousIndex = accEnergy[previousIndex].edgeTo;
        i++;
    }
}

void removeSeam(Img *img, int* seam)
{
    for(int i = 0; i < img->height; i++) {
        int lineEnd = seam[i] / img->width;
        if(seam[i] % img->width > 0)
            lineEnd++;
        lineEnd = lineEnd * img->width;

        for(int j = seam[i]; j < lineEnd-1; j++) {
            img->img[j].r = img->img[j+1].r;
            img->img[j].g = img->img[j+1].g;
            img->img[j].b = img->img[j+1].b;
        }

        img->img[lineEnd-1].r = -1;
        img->img[lineEnd-1].g = -1;
        img->img[lineEnd-1].b = -1;
    }

    // Reorganiza os pixels para o novo tamanho
    for(int i = 0; i < img->width*img->height; i++) {
        if(i % img->width < img->width-1) {
            int line = i / img->width;
            img->img[i-line].r = img->img[i].r;
            img->img[i-line].g = img->img[i].g;
            img->img[i-line].b = img->img[i].b;
        }
    }

    img->width--;


//    for(int i = 0; i < img->height; i++) {
//        img->img[seam[i]].r = 255;
//        img->img[seam[i]].g = 0;
//        img->img[seam[i]].b = 0;
//    }

}

void crop(Img *img, Img mask, int pixels)
{
    for(int k = 0; k < pixels; k++) {
        long energy[img->height * img->width];
        int seam[img->height];
        computeEnergy(*img, mask, energy);
        computeSeam(*img, energy, seam);
        removeSeam(img, seam);
    }
}

void imageCopy(Img in, Img *out)
{
    out->height = in.height;
    out->width = in.width;
    for(int i = 0; i < in.height * in.width; i++) {
        out->img[i].r = in.img[i].r;
        out->img[i].g = in.img[i].g;
        out->img[i].b = in.img[i].b;
    }
}
