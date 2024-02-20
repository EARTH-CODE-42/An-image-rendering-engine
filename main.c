#include <windows.h>
#include <stdio.h>
#include <pthread.h>//Ϊִ�д��뵥�������߳�
#include <float.h>
#include <setjmp.h>//���ڷǾֲ�goto�����쳣
#include <time.h>
#include <math.h>
//=====================================================================
// Win32 ���ڼ�ͼ�λ���
//=====================================================================
#define WINDOW_W 1920//�������Ĵ��ڳߴ��С
#define WINDOW_H 1080
int draw_w, draw_h, screen_exit = 0;
const int screen_w,screen_h;
int mouse_x,mouse_y;                    //�������
int mouse_lclick=0;                     //�������Ƿ���
int mouse_have_lclick=0;                //�������Ƿ��Ѿ����£�ִ��һ��
int mouse_rclick=0;                     //����Ҽ��Ƿ���
int mouse_have_rclick=0;                //����Ҽ��Ƿ��Ѿ����£�ִ��һ��
int screen_keys[512];	                //��ǰ���̰���״̬
int is_press=0;                         //�����Ƿ��Ѿ�����
int press_time=0;                       //���̰���ʱ��
static HWND screen_handle = NULL;       //������ HWND
static HDC screen_dc = NULL;            //�������ڴ��豸������ HDC
static HBITMAP screen_hb = NULL;        //������λͼ DIB
unsigned int *screen_fb = NULL;        //������frame buffer
static HWND draw_handle = NULL;         //�滭�� HWND
static HDC draw_dc = NULL;              //�滭�� HDC
static HBITMAP draw_hb = NULL;          //�滭��λͼ DIB
unsigned int *draw_fb = NULL;          //�滭��frame buffer

int win32_init(int w, int h, char *title);	// ��Ļ��ʼ��
void win32_close(void);					// �ر���Ļ
void win32_dispatch(void);					// ������Ϣ
// win32 event handler
static LRESULT win32_events(HWND, UINT, WPARAM, LPARAM);

static LRESULT win32_events(HWND hWnd, UINT uMsg,WPARAM wParam, LPARAM lParam)
{//6.������Ϣ�Ĵ������
	HDC hdc;
    switch (uMsg)
    {
    case WM_CLOSE:
        screen_exit = 1;
        break;
    case WM_LBUTTONDOWN://���
		mouse_lclick=1;
		break;
	case WM_LBUTTONUP:
		mouse_lclick=0;
        mouse_have_lclick=0;
		break;
    case WM_RBUTTONDOWN://�Ҽ�
        mouse_rclick=1;
		break;
    case WM_RBUTTONUP:
		mouse_rclick=0;
        mouse_have_rclick=0;
		break;
	case WM_MOUSEMOVE://����ƶ��¼�
		hdc=GetDC(hWnd);//��ʼ��HDC
		mouse_x=LOWORD(lParam);//���ĺ�����
		mouse_y=HIWORD(lParam);//����������
		ReleaseDC(hWnd,hdc);//�������ߴ��ţ��ͷ�HDC
		break;
    case WM_KEYDOWN:
        screen_keys[wParam & 511] = 1;
        break;
    case WM_KEYUP:
        screen_keys[wParam & 511] = 0;
        is_press=0;
        press_time=0;
        break;
    default:
        //����Ĭ�Ϸ�ʽ�������Ϣ���ط�0ֵ
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;//������ʽ�������Ϣ����0ֵ
}

void win32_dispatch(void)
{//5.���봰�ڵ���Ϣѭ��
    MSG msg;//������������Ϣ���Ƶ�MSG����
    while (1){
        //�ӵ�ǰ�߳���Ϣ���м�����1����Ϣ�����Ƶ�msg������������Ϣ������ɾ��
        if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
            break;//��⵽WM_QUIT��Ϣ���˳�
        if (!GetMessage(&msg, NULL, 0, 0))//�����߳����������ŵ���Ϣ
            break;
        //�������������������Ϣ
        DispatchMessage(&msg);//����Ϣ�ַ���win32_events()���ڹ��̽��д���
    }
}
// ��ʼ�����ڲ����ñ���
int win32_init(int w, int h, char *title)
{
    WNDCLASSA wc = {CS_BYTEALIGNCLIENT, (WNDPROC)win32_events,//�󶨴��ڵ���Ϣ������
                    0, 0, 0,NULL, NULL, NULL, NULL, title//��������
                   };//1.����������
    BITMAPINFO bi = { {
            sizeof(BITMAPINFOHEADER), WINDOW_W, -WINDOW_H, 1, 32, BI_RGB,
            WINDOW_W * WINDOW_H * 4, 0, 0, 0, 0
        },{{0,0,0,0}}
    };
    RECT rect = { 0, 0, w, h };
    int wx, wy, sx, sy;
    LPVOID ptr;

    win32_close();

    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);//���ڱ���ɫ
    wc.hInstance = GetModuleHandle(NULL);//������������Ӧ�ó�����
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);//�����ʾΪʮ�ֹ��
    if (!RegisterClass(&wc)) return -1;//2.ע�ᴰ����

    screen_handle = CreateWindow(title, "EDITOR",//3.��ע��Ĵ����ഴ��ʵ������
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,//���ڵ���ʽ
                                  0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

    draw_handle = CreateWindow(title, "PLOTTER",//�����ɹ����ش��ھ��
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
    if (screen_handle == NULL) return -2;
    screen_exit = 0;
    HDC hDC;
    /***�����ڻ�ͼ����***/
    hDC = GetDC(screen_handle);
    //����һ����ָ���豸���ݵ��ڴ��豸�����Ļ���DC
    screen_dc = CreateCompatibleDC(hDC);
    ReleaseDC(screen_handle, hDC);
    //����Ӧ�ó������ֱ��д������豸�޹ص�λͼDIB
    screen_hb = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
    if (screen_hb == NULL) return -3;
    SelectObject(screen_dc, screen_hb);//��λͼ�������õ���ǰ�豸��
    screen_fb = (unsigned int*)ptr;
    /***�滭����ͼ����***/
    hDC = GetDC(draw_handle);
    //����һ����ָ���豸���ݵ��ڴ��豸�����Ļ���DC
    draw_dc = CreateCompatibleDC(hDC);
    ReleaseDC(draw_handle, hDC);
    //����Ӧ�ó������ֱ��д������豸�޹ص�λͼDIB
    draw_hb = CreateDIBSection(draw_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
    if (draw_hb == NULL) return -3;
    SelectObject(draw_dc, draw_hb);//��λͼ�������õ���ǰ�豸��
    draw_fb = (unsigned int*)ptr;

    draw_w = w;
    draw_h = h;
    AdjustWindowRect(&rect, GetWindowLong(screen_handle, GWL_STYLE), 0);
    wx = rect.right - rect.left;
    wy = rect.bottom - rect.top;
    sx = (GetSystemMetrics(SM_CXSCREEN) - wx) / 2;
    sy = (GetSystemMetrics(SM_CYSCREEN) - wy) / 2;
    if (sy < 0) sy = 0;
    SetWindowPos(screen_handle, NULL, sx-w/2, sy, wx, wy, (SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
    SetWindowPos(draw_handle, NULL, sx+w/2, sy, wx, wy, (SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW));
    SetForegroundWindow(screen_handle);

    ShowWindow(screen_handle, SW_NORMAL);//4.��ʾ����
    win32_dispatch();

    memset(screen_keys, 0, sizeof(int) * 512);
    memset(screen_fb, 0, screen_w * screen_h * 4);
    memset(draw_fb, 0, screen_w * screen_h * 4);
    return 0;
}

void win32_close(void)
{
    if (screen_dc){
        DeleteDC(screen_dc);
        screen_dc = NULL;
    }
    if (screen_hb){
        DeleteObject(screen_hb);
        screen_hb = NULL;
    }
    if (screen_handle){
        CloseWindow(screen_handle);
        screen_handle = NULL;
    }
    if (draw_dc){
        DeleteDC(draw_dc);
        draw_dc = NULL;
    }
    if (draw_hb){
        DeleteObject(draw_hb);
        draw_hb = NULL;
    }
    if (draw_handle){
        CloseWindow(draw_handle);
        draw_handle = NULL;
    }
}
/******************************THGTTG*********************************/
/******************************ȫ�ֱ���*******************************/
#define PI 3.1415926f
#define BUFFER_SIZE 1024*8  //��������С
int draw_fps = 0;//�滭����fps
const int screen_w = 640;//�����ڳߴ�
const int screen_h = 480;
int car_run = 1;//�Ƿ�������
int frame = 1;//֡����
jmp_buf jump;//���ñ���Ա������﷨���������ת
int wrong = 0;//����ִ���Ƿ������˴���?
int wrong_type = 0;//�����������������Է�
int wrong_row,wrong_col;//���������С���
unsigned int plotter_color=0x00ff7f27;//��ǰ��ͼ����ɫ
int plotter_x=0,plotter_y=0;//��ǰ��ͼ����ʼ��
int plotter_font=18;//�����С
#if 0
char code[BUFFER_SIZE]="\
graph(x,y){\n\
   #if(x>0){\n\
   #   return(x);\n\
   #}\n\
   #return(tan(x/20)*20);\n\
   return(sin(x*y));\n\
}\n\
color(rgb(255,255,120));\n\
show();\n\
str='world';\n\
text('hello ',str,'\\n');\n\
num=34;\n\
b=12.3;\n\
text(num-b+3*4/6,'\\n');\n\
move(100,0);#test\n\
color(rgb(200,200,0));\n\
text('4^2-3=',4^2-3);\n\
move(winw/2,winh/2);\n\
line(mousx,mousy);\n\
color(rgb(34,178,76));\n\
circle(mousx/2,mousy/2,sin(clock/100)*100);\n\
winw=800;\n\
winh=600;\n\
func test(n){\n\
   if(n==0){\n\
      return(1);\n\
   }else{\n\
      return(n*test(n-1));\n\
   }\n\
}\n\
n=5;\n\
move(0,100);\n\
text(test(n),'\\n');\n\
array ar(10);\n\
i=0;\n\
while(i<10){\n\
   n=i;\n\
   ar[i]=test(n);\n\
   text(ar[i],'\\n');\n\
   i=i+1;\n\
}\n\
text(ar[4]);\n\
color(rgb(rand,rand,rand));\n\
font(100);\n\
move(winw/2-100,winh/2-100);\n\
text('Thank you');";//"abc=a+b-d;\ntext(a,'a','b','haha\nha');"
#else
char code[BUFFER_SIZE]="";//"func fun(m){\ntext(m);#return(m);\n}\nd=3;\nfun(6);#text(fun(d));\nfun();";
#endif
int node=0;//������
int cursor=0;//���
int cursor_r=0;//�������
int cursor_c=0;//�������
int line_start=0;//������ʾ��ʼ��
enum {//����
    KEY_BAK=8,KEY_TAB,KEY_ENTER=13,KEY_DEL=46,KEY_SEM=186,KEY_PLU,KEY_LES,KEY_SUB,KEY_GRE,KEY_QUE,KEY_LB=219,KEY_SLASH,KEY_RB,KEY_QUO,
    KEY_0=48,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,
    KEY_A=65,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z,
    };
/****************************��������****************************/
//ABS
#define ABS(x) ((x)<=0?-(x):(x))
//SIGN
#define SIGN(x) (x > 0 ? 1 : (x < 0 ? -1 : 0))
//RGBתr,g,b
#define CRGB(a,b,c) (((a)<<16)|((b)<<8)|(c))
//�ж��Ƿ�����Ļ�ڲ�
int in_screen(int,int,int);
inline int in_screen(int a,int b,int w){//w����ָʾ��ǰ��Ҫ�ڼ��Ŵ��ڻ���
    if(w==0)//������
        return ((a)>=0&&(b)>=0&&(a)<screen_w&&(b)<screen_h);
    else//��ͼ��
        return ((a)>=0&&(b)>=0&&(a)<draw_w&&(b)<draw_h);
}
//�������ص��봰�ڳߴ��൱����Ļ����
//screen_fb[((y)<<9)+((y)<<7)+(x)]=c
void draw_pixel(int,int,unsigned int,int);
inline void draw_pixel(int y,int x,unsigned int c,int w){
    if(in_screen(x,y,w)){
        if(w==0)
            screen_fb[y*WINDOW_W+x]=c;
        else
            draw_fb[y*WINDOW_W+x]=c;//�ı䴰�ڳߴ�ֻ�������Ͻ�һ��С�������
    }
}
//���������
int fequal(double,double);
inline int fequal(double a,double b){
    return (ABS((a)-(b))<1e-9);
}
int get_fps();
/****************************ͼ�����****************************/
int find_region(int x, int y,int win){//Cohen-Sutherland�����㷨
  int region=0;
  int h=win?draw_h:screen_h;
  int w=win?draw_w:screen_w;
  if(y >= h)
    region |= 1; //top
  else if( y < 0)
    region |= 2; //bottom
  if(x >= w)
    region |= 4; //right
  else if ( x < 0)
    region |= 8; //left
  return region;
}
//��2Dֱ�߲ü�����Ļ�ռ�,9���������������:
//9 8 10
//1 0 2
//5 4 6
void clip_line(int x1, int y1, int x2, int y2, int *x3, int *y3, int *x4, int *y4,int win)
{//w������ʾ��ǰ������ĸ�����
  int code1, code2, codeout;
  int accept = 0, done=0;
  int h=win?draw_h:screen_h;
  int w=win?draw_w:screen_w;
  code1 = find_region(x1, y1, win); //the region outcodes for the endpoints
  code2 = find_region(x2, y2, win);
  do  //In theory, this can never end up in an infinite loop, it'll always come in one of the trivial cases eventually
  {
    if(!(code1 | code2)) accept = done = 1;  //accept because both endpoints are in screen or on the border, trivial accept
    else if(code1 & code2) done = 1; //the line isn't visible on screen, trivial reject
    else{ //if no trivial reject or accept, continue the loop
      int x, y;
      codeout = code1 ? code1 : code2;
      if(codeout & 1){//top
        x = x1 + (x2 - x1) * (h - y1) / (y2 - y1);
        y = h - 1;
      }
      else if(codeout & 2){//bottom
        x = x1 + (x2 - x1) * -y1 / (y2 - y1);
        y = 0;
      }
      else if(codeout & 4){//right
        y = y1 + (y2 - y1) * (w - x1) / (x2 - x1);
        x = w - 1;
      }
      else{//left
        y = y1 + (y2 - y1) * -x1 / (x2 - x1);
        x = 0;
      }
      if(codeout == code1){//first endpoint was clipped
        x1 = x; y1 = y;
        code1 = find_region(x1, y1, win);
      }
      else{//second endpoint was clipped
        x2 = x; y2 = y;
        code2 = find_region(x2, y2, win);
      }
    }
  }
  while(done == 0);
  if(accept){
    *x3 = x1;
    *x4 = x2;
    *y3 = y1;
    *y4 = y2;
  }else
    *x3 = *x4 = *y3 = *y4 = 0;
}
void draw_hor_line(int y,int x1,int x2,unsigned int c,int w)//��ˮƽ��
{
    if(!in_screen(x1,y,w)||!in_screen(x2,y,w))//������Ļ��ʲôҲ����
        return;
    //ȷ�����ƴ���
    unsigned int *buffer=w?draw_fb:screen_fb;
    unsigned int *p1 = buffer+y*WINDOW_W+x1;
    unsigned int *p2 = buffer+y*WINDOW_W+x2;
    while (p1<=p2){
        *p1 = c;
        p1++;
    }
    draw_pixel(y,x1,RGB(34,212,153),w);
}
void draw_ver_line(int x,int y1,int y2,unsigned int c,int w)//����ֱ��
{
    //ȷ�����ƴ���
    int win_h=w?draw_h:screen_h;
    int win_w=w?draw_w:screen_w;
    unsigned int *buffer=w?draw_fb:screen_fb;
    if(x>=win_w||x<0) return;
    //������ĻҲ�ܻ����ֵ�
    if(y1>y2){//����y1 y2
        y1+=y2;
        y2=y1-y2;
        y1-=y2;
    }
    y1=y1<0?0:y1;
    y2=y2>=win_h?win_h-1:y2;
    if(y1>=win_h||y2<0) return;
    buffer=buffer+y1*WINDOW_W+x;
    while (y1<=y2){
        *buffer=c;
        buffer+=WINDOW_W;
        y1++;
    }
}
void draw_line(int x1, int y1, int x2, int y2, unsigned int color,int w)//Bresenham������
{
    if(!in_screen(x1,y1,w)||!in_screen(x2,y2,w))
        clip_line(x1,y1,x2,y2,&x1,&y1,&x2,&y2,w);
    int deltax = ABS(x2 - x1); //����֮��Ĳ�ֵ
    int deltay = ABS(y2 - y1);
    int x = x1;//�����
    int y = y1;
    int xinc1, xinc2, yinc1, yinc2, den, num, numadd, numpixels, curpixel;
    if(x2 >= x1){//xֵ�ڲ�������
        xinc1 = 1;
        xinc2 = 1;
    }else{//x�ڳ�������
        xinc1 = -1;
        xinc2 = -1;
    }
    if(y2 >= y1){//yֵ�ڲ�������
        yinc1 = 1;
        yinc2 = 1;
    }else{//y�ڳ�������
        yinc1 = -1;
        yinc2 = -1;
    }
    if(deltax >= deltay){//��������y��������ҵ���Ӧx����
        xinc1 = 0;//������>=��ĸ���ı�x
        yinc2 = 0;//ÿ�ε��������ı�y
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;//x����y
    }else{//��������x��������ҵ���Ӧy����
        xinc2 = 0;//ÿ�ε��������ı�x
        yinc1 = 0;//������>=��ĸ���ı�y
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;//y����x
    }
    for(curpixel = 0; curpixel <= numpixels; curpixel++){
        draw_pixel(y, x, color, w);//���Ի�����
        num += numadd;//����С���ķ��Ӳ���
        if (num >= den){//����>��ĸ
            num -= den;//�����·�ĸ
            x += xinc1;//����x
            y += yinc1;//����y
        }
        x += xinc2;//����x
        y += yinc2;//����y
    }
}
#if 0
void draw_curve(int *points,int num, unsigned int color,int w){//������������ //��ʵ��
    if(num<1) return;
    if(num==1)draw_pixel(points[1],points[0],c,w);
    for(int i=0;i<num;i++){
        int size=len;
        int parent=temp;
        while(size>1){

        }
    }
}
#endif // 0
void draw_rect(int left,int top,int right,int bottom,unsigned int c,int w){//��ʵ�ľ���
    int i,j;
    left=left<0?0:left;
    top=top<0?0:top;
    right=right<screen_w?right:screen_w-1;
    bottom=bottom<screen_h?bottom:screen_h-1;
    for(i=top;i<=bottom;i++)
        for(j=left;j<=right;j++)
            draw_pixel(i,j,c,w);
}
void draw_circle(int xc,int yc,int radius,unsigned int color,int w){//bresenham����Բ
    int t=radius>=0?1:0;
    radius=ABS(radius);
    int x = 0;//�뾶����ʵ��Բ,��������Բ
    int y = radius;
    int p = 3-(radius<<1);
    int a,b,c,d,e,f,g,h;
    int pb = yc + radius + 1, pd = yc + radius + 1;//ǰһ��ֵ,��ֹ�ظ�����
    if(!t)//����Բ
        while(x<=y){
            a = xc + x;//���ڶԳ����ȵõ�8����
            b = yc + y;
            c = xc - x;
            d = yc - y;
            e = xc + y;
            f = yc + x;
            g = xc - y;
            h = yc - x;
            draw_pixel(b, a, color, w);
            draw_pixel(d, c, color, w);
            draw_pixel(f, e, color, w);
            draw_pixel(f, g, color, w);
            if(x > 0){//��ֹ��ͬһ���ط�����
                draw_pixel(d, a, color, w);
                draw_pixel(b, c, color, w);
                draw_pixel(h, e, color, w);
                draw_pixel(h, g, color, w);
            }
            if(p < 0) p += (x++ << 2) + 6;
            else p += ((x++ - y--) << 2) + 10;
        }
    else//ʵ��Բ
        while (x <= y){
            a = xc + x;
            b = yc + y;
            c = xc - x;
            d = yc - y;
            e = xc + y;
            f = yc + x;
            g = xc - y;
            h = yc - x;
            if(b != pb) draw_ver_line(a, b, d, color, w);
            if(d != pd) draw_ver_line(c, b, d, color, w);
            if(f != b)  draw_ver_line(e, f, h, color, w);
            if(h != d && h != f) draw_ver_line(g, f, h, color, w);
            pb = d;
            pd = b;
            if(p < 0) p += (x++ << 2) + 6;
            else p += ((x++ - y--) << 2) + 10;
        }
}
int draw_text(int x,int y,int h,unsigned int c,char *s,int w){//���Ըı������С,���ػ��Ƶ��ַ���
    int i=0;//ע��RGB��r��bҪ����λ�ò���CRGB,��TextOut�õ���RGB
    HDC Hdc=w?draw_dc:screen_dc;//�����Ӧ������ʱ�ڴ�������
    HFONT hFont;    //������
    h=(h==0)?18:h;  //����Ĭ�ϴ�С18��
    hFont = CreateFont(
        h,0,    //�߶�h, ��ȡ0��ʾ��ϵͳѡ�����ֵ
        0, 0,    //�ı���б��������б��Ϊ0
        FW_BOLD,    //����
        0,0,0,        //��б�����»������л���
        ANSI_CHARSET,    //�ַ���
        OUT_DEFAULT_PRECIS, //ָ���������
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,        //һϵ�е�Ĭ��ֵ
        DEFAULT_PITCH | FF_DONTCARE,
        "Consolas"    //��������
    );
    //Hdc�ǻ�ͼ���� hFont�������� hDC�Ǵ��ھ�� screen_handle�������ڣ�
    SetBkMode(Hdc,TRANSPARENT);
    SetTextColor(Hdc,c);
    SelectObject(Hdc,hFont);//�������õ���ǰ�豸��
    while(1){
        while(s[i]!=0&&s[i]!='\n')
            i++;//�����ƻ��з�
        TextOut(Hdc, x, y, s, i);
        if(s[i]=='\n'){
            if(w)//�ı�������
                plotter_y+=h;
            s=s+i+1;
            i=0;
            y+=h;//���н���д
        }else
            break;
    }
    DeleteObject(hFont);//ɾ������������
    return i;
}
/****************************����ͼ��****************************/
double origin_x=640/2;//����ԭ��λ�ڴ�������ϵ�Ĵ�������
double origin_y=480/2;
double coord_ratio=0.07;//���ű���
double coord_value=0.0;//���λ�õ�ֵ
int graph_slow=0;//�Ƿ������ӳ���Ⱦģʽ
//��������תֱ������
void win_coor(double,double,double*,double*);
inline void win_coor(double wx,double wy,double *cx,double *cy){
    *cx=(wx-origin_x)*coord_ratio;
    *cy=(origin_y-wy)*coord_ratio;
}
//ֱ������ת��������
void coor_win(double,double,int*,int*);
inline void coor_win(double cx,double cy,int *wx,int *wy){
    *wx=(int)(cx/coord_ratio+origin_x);
    *wy=(int)(origin_y-cy/coord_ratio);
}
/****************************�ʷ�����****************************/
#define SYMBOL_SIZE 1024*8  //���ű��С
#define DATA_SIZE 1024      //�����õ����ַ����������С
#define NAME_SIZE 32        //��ʶ������
#define PARAMETER_SIZE 8    //������������
#define CURSE_DEEP 42       //���ݹ����
#define RETURNFLAG DBL_MAX  //������������ֵ��
typedef struct symbol{
    int type;               //��ʶ����ֵ����������
    char name[NAME_SIZE];   //��ʶ������
    double value;           //����,����,���鳤��
    char* funcp;            //���ں�����Ǻ���Դ����λ�� �����ַ�����ʾ��ʼ��ַ
    double* list;           //ָ������
    struct symbol *car;     //ָ���������ı�ʶ��
    struct symbol *cdr;     //ָ���������ı�ʶ��
    int depth;              //�������
}symbol;//����
typedef struct context{
    int token;
    char *locat;
}context;//����ִ��λ�õ�������
enum{                                                               //ö�����Է�����
    End=128, Num, Str, Array, Func, Car, Cdr, Val,                  //��������
    Else, If, Return, While, Clock, Mousx, Mousy, Winw, Winh, Rand, //��ֵ����
    Pi, E,                                                          //���ó���
    Text, Read, Sqrt, Cos, Sin, Abs, Tan ,Log, Floor,               //��ֵ����
    Color, Pixel, Rgb, Move, Line, Rect, Circle, Font, Flush,       //��ͼ����
    Graph, Show, Slow,                                              //����ͼ��
    Assign, OR, AND, Equal, Sym, FuncSym, ArraySym, Void,           //��Ԫ�����
    Nequal, LessEqual, GreatEqual, Inc, Dec                         //һԪ�����
};//��Array��Circle�����ùؼ���,����ǰ������ű�
int token_type;                 //��ǰ���Է�����
double token_value;             //���Է�ֵ ���Ρ�����
char *token_str;                //���Է���ַ ָ���ַ���
symbol *token_sym;              //���Է���ַ ָ���ʶ��
symbol symbol_list[SYMBOL_SIZE];//��ʶ����
int symbol_num = 0;             //��ʶ��������,�ȴ��ټ�
char data[DATA_SIZE]="";        //�ַ��������鼯
int data_num = 0;               //��ǰ�ַ�������
int curse_depth = 0;            //�ݹ����
char *pode = 0;                 //��ǰ���ڴ���Ĵ���
char *rode = 0;                 //�Ѿ�ȷ������ȷ�Ĵ���,������ʾ��������λ��
double return_value = 0.0;      //�����ķ���ֵ
double cycle = 0.0;             //����������
void find_symbol(char *name){//�ڱ�ʶ�����ұ�ʶ������,�Ҳ����ͼ���
    for(int i=symbol_num-1;i>=0;i--){//�ڱ�ʶ������������������Ƿ���ֹ� ��������Ϊ������ƥ�亯���ݹ����ͬ����ʶ�������������
        if(strcmp(name,symbol_list[i].name)==0){//�ҵ���
            if(symbol_list[i].type==Num){//����ʶ���滻Ϊ�ҵ���ֵ
                if(symbol_list[i].depth!=0&&symbol_list[i].depth!=curse_depth)//��ȫ�ֱ����ұ���������һ��,��Ϊ�±���
                    break;
                token_sym=symbol_list+i;
                token_type=Sym;//��ֵ��ʶ��
            }
            else if(symbol_list[i].type==FuncSym){//��ʶ����һ����������
                token_sym=symbol_list+i;
                token_type=FuncSym;//������ʶ��
            }
            else if(symbol_list[i].type==ArraySym){
                token_sym=symbol_list+i;
                token_type=ArraySym;//�����ʶ��
            }
//            else if(symbol_list[i].type==ConsSym){
//                token_sym=symbol_list+i;
//                token_type=ConsSym;//�б��ʶ��
//            }
            else if(symbol_list[i].type==Str){
                token_str=symbol_list[i].funcp;
                token_type=Str;//�ַ�����ʶ��
            }else{
                if(symbol_list[i].type==Void){
                    token_type=Sym;
                    token_sym=symbol_list+i;
                }
                else token_type=symbol_list[i].type;//ϵͳ�Դ�����
            }
            return;
        }
    }
    strcpy(symbol_list[symbol_num].name,name);//��ʶ��֮ǰû��,���Ǽ����ʶ����
    symbol_list[symbol_num].depth=curse_depth;//�ݹ����ȷ���������õı���ֻ�Ծֲ��ɼ�
    symbol_list[symbol_num].type=Void;
    token_sym=symbol_list+symbol_num;
    symbol_num++;
    token_type=Sym;
}
void match(int);
void next_token(){//��ȡ��һ�����
    while(*pode){
        if(*pode == '#'){//ע��
            while (*pode != 0 && *pode != '\n')
                pode++;
        }
        else if(*pode==' '||*pode=='\n'||*pode=='\t')
            pode++;
        else if(*pode=='*'||*pode=='/'||*pode==';'||*pode=='%'||*pode=='^'||*pode==','||*pode=='('||*pode==')'||*pode=='{'||*pode=='}'||*pode=='['||*pode==']'||*pode=='.'){
            token_type=*pode++;//��������Ϊ���Է�
            return;
        }
        else if(*pode>='0'&&*pode<='9'){//�����򸡵� ����ʾΪ����
            token_value=0.0f;
            while (*pode>='0'&&*pode<='9'){
                token_value = token_value * 10.0f + *pode++ - '0';
            }
            if(*pode=='.'){//����
                double digits=10.0f;
                pode++;
                while(*pode>='0'&&*pode<='9'){//����С����
                    token_value=token_value+((double)(*pode++)-'0')/digits;
                    digits*=10.0f;
                }
            }
            token_type=Num;
            return;
        }
        else if(*pode=='\''){//�ַ���
            token_str=data+data_num;//�ַ������洢����ʼ��ַ
            pode++;//�ַ�������
            while (*pode!=0&&*pode!='\''){//�����ַ���
                data[data_num]=*pode;
                if(*pode=='\\'){//ת���ַ�
                    pode++;
                    if(*pode=='n')
                        data[data_num]='\n';
                }
                pode++;
                data_num++;
            }
            data[data_num++]=0;
            token_type=Str;
            pode++;
            return;
        }
        else if((*pode>='a'&&*pode<='z')||(*pode>='A'&&*pode<='Z')||(*pode=='_')){//��ʶ��
            char name[NAME_SIZE];//��ʱ�洢��ʶ��
            int name_num=0;
            while((*pode>='a'&&*pode<='z')||(*pode>='A'&&*pode<='Z')||(*pode>='0'&&*pode<='9')||(*pode=='_')){
                name[name_num]=*pode;
                pode++;
                name_num++;
            }
            name[name_num]=0;//��ʶ������
            find_symbol(name);//���ұ�ʶ��˳�����token
            return;
        }
        else if(*pode=='='){//��� ��ֵ
            pode++;
            token_type='=';//��ֵ
            if(*pode=='='){
                pode++;
                token_type=Equal;//���
            }
            return;
        }
        else if(*pode=='+'){//�� ����
            pode++;
            token_type='+';
            if(*pode=='+'){
                pode++;
                token_type=Inc;
            }
            return;
        }
        else if(*pode=='-'){//�� �Լ�
            token_type='-';
            pode++;
            if(*pode=='-'){
                pode++;
                token_type=Dec;
            }
            return;
        }
        else if(*pode=='!'){//������
            pode++;
            if(*pode=='='){
                pode++;
                token_type=Nequal;
            }
            return;
        }
        else if(*pode=='<'){//С�� С�ڵ���
            token_type='<';
            pode++;
            if (*pode=='='){
                pode++;
                token_type=LessEqual;
            }
            return;
        }
        else if(*pode=='>'){//���� ���ڵ���
            pode++;
            token_type='>';
            if(*pode=='='){
                pode++;
                token_type=GreatEqual;
            }
            return;
        }
        else if(*pode=='|'){//������ �߼���
            pode++;
            token_type='|';
            if (*pode=='|'){
                pode++;
                token_type=OR;
            }
            return;
        }
        else if(*pode=='&'){//������ �߼���
            pode++;
            token_type='&';
            if (*pode=='&'){
                pode++;
                token_type=AND;
            }
            return;
        }else
            match(End);//�϶��Ǹ����
    }
    token_type=End;
}
//����ָ��Դ�����λ��ָ�뷵�ش����Ӧ������
void get_posion(char *ptr,int *row,int *col){
    /**���ڿ��µĶ��߳�,����ֱ��ʹ�ö��߳�ͬʱʹ�õ�ȫ�ֱ���,ֻ��һ���Ը�ֵ**/
    int c=1,r=1;
    char *t=code;
    while(*t&&t!=ptr){
        c++;
        if(*t=='\n'){
            c=0;
            r++;
        }
        t++;
    }
    *row=r;
    *col=c;
}
//������Է��Ƿ�ƥ��˳�����﷨����
void match(int token){//�������Ϊ������������,���ǵõ���һ������
    if(token_type==token){
        rode=pode;
        next_token();//������һ�����Է�
    }
    else{
        wrong=token;
        wrong_type=token_type;
        get_posion(rode,&wrong_row,&wrong_col);//��ȡ����ص�
        longjmp(jump,1);//��ת��������,����ִ�к�������
    }
}
/****************************�﷨����****************************/
double expression();
double factor();
double term();
double function();
int boolOR();
//�ݹ������ʽ
double term(){//����˳�����
    double temp=factor();
    while(token_type=='*'||token_type=='/'||token_type=='^'){
        if(token_type=='*'){
            match('*');
            temp*=factor();
        }
        else if(token_type=='/'){
            match('/');
            double divid=factor();
            temp=fequal(divid,0.0)?0.0:temp/divid;//�������Ϊ0,���Ϊ0
        }else{
            match('^');
            temp=pow(temp,factor());
        }
    }
    return temp;
}
double factor(){//��ʶ�������ú�����ֵ
    double temp=0;
    if(token_type=='('){
        match('(');
        temp=expression();//������ʽ�����ֵ
        match(')');
    }
    else if(token_type==Num){
        temp=token_value;
        match(Num);
    }
    else if(token_type==Sym){//��ʶ��
        temp=token_sym->value;
        match(Sym);
    }
    else if(token_type==FuncSym){
        return function();//������ֵ
    }
    else if(token_type==ArraySym){
        symbol* ptr=token_sym;
        match(ArraySym);
        match('[');//�ڴ����Է�[ ��ƥ��ȡ��һ�����Է�
        int index=(int)expression();//����[]�����ڵ���ֵ
        if(index>=0&&index<ptr->value)//�±�δԽ��
            temp=ptr->list[index];
        match(']');
    }
//    else if(token_type==ConsSym){//�б���ֵ
//        symbol* ptr=token_sym;
//        match(ConsSym);
//        if(token_type=='.'){
//            match('.');
//            if(token_type==Car){//������
//                match(Car);
//                if(ptr->funcp==NULL)//��ͼ����δ��ʼ���Ķ���
//                    temp=0.0;
//                else
//                    temp=((symbol*)ptr->funcp)->value;
//            }else if(token_type==Cdr){//������
//                match(Cdr);
//                if(ptr->list==NULL)//��ͼ����δ��ʼ���Ķ���
//                    temp=0.0;
//                else
//                    temp=((symbol*)ptr->list)->value;
//            }
//        }else{//�б���ķ���ֵ�������ڱ�ʶ����ĵ�ַ
//            temp=ptr->value;
//        }
//    }
    else if(token_type==Sin){//�������ú�����sin
        double tmp;
        match(Sin);
        match('(');
        tmp=expression();
        match(')');
        return sin(tmp);//���ؼ�����
    }else if(token_type==Cos){//�������ú�����cos
        double tmp;
        match(Cos);
        match('(');
        tmp=expression();
        match(')');
        return cos(tmp);//���ؼ�����
    }else if(token_type==Tan){//�������ú�����tan
        double tmp;
        match(Tan);
        match('(');
        tmp=expression();
        match(')');
        return tan(tmp);//���ؼ�����
    }else if(token_type==Log){//�������ú�����log
        double x,y;
        match(Log);
        match('(');
        x=expression();
        match(',');
        y=expression();
        match(')');
        return log(y)/log(x);//����xΪ��y�Ķ���
    }else if(token_type==Floor){//�ذ庯��
        double x;
        match(Floor);
        match('(');
        x=expression();
        match(')');
        return (long long)x;//����xΪ��y�Ķ���
    }else if(token_type==Rgb){//rgb��ɫ���뱣����24bit�Ŀռ�
        int r,g,b;
        double ret=0.0;
        match(Rgb);
        match('(');
        r=(int)expression();
        match(',');
        g=(int)expression();
        match(',');
        b=(int)expression();
        match(')');
        ret=CRGB(r%256,g%256,b%256);
        return ret;
    }else if(token_type==Sqrt){//�������ú�����sqrt
        double tmp;
        match(Sqrt);
        match('(');
        tmp=expression();
        match(')');
        return sqrt(tmp);//���ؼ�����
    }else if(token_type==Abs){//�������ú�����abs
        double tmp;
        match(Abs);
        match('(');
        tmp=expression();
        match(')');
        return ABS(tmp);//���ؼ�����
    }else if(token_type==Clock){
        match(Clock);
        temp=cycle;
    }else if(token_type==Mousx){
        match(Mousx);
        temp=mouse_x;
    }else if(token_type==Mousy){
        match(Mousy);
        temp=mouse_y;
    }else if(token_type==Winw){
        match(Winw);
        temp=draw_w;
    }else if(token_type==Winh){
        match(Winh);
        temp=draw_h;
    }else if(token_type==Pi){
        match(Pi);
        temp=3.1415926535;
    }else if(token_type==E){
        match(E);
        temp=2.7182818284;
    }else if(token_type==Rand){
        match(Rand);
        temp=rand();
    }
    return temp;
}
double expression(){//����([���ʽ])��ֵ
    double temp=term();//���������ȼ�����������
    while(token_type=='+'||token_type=='-'||token_type=='%'){//ֻ��ԼӼ�,���õ��������Է���])�򷵻�
        if(token_type=='+'){
            match('+');//��ȡ��һ�����Է�
            temp+=term();//�������ȼ����ߵ� �˳�����ֵ ������ ĳ����ʶ����ֵ
        }
        else if(token_type=='-'){
            match('-');
            temp-=term();
        }else{
            match('%');
            int cdr=(int)term();
            if(cdr==0)//�ɲ���ģ0��
                temp=0;
            else
                temp=(int)temp%cdr;
        }
    }
    return temp;
}
int boolexp(){//���㲼��ֵ
    if(token_type=='('){//if()
        match('(');
        int result=boolOR();
        match(')');
        return result;
    }
    else if(token_type=='!'){
        match('!');
        return boolexp();
    }
    double temp = expression();
    if(token_type=='>'){
        match('>');
        return temp>expression();
    }
    else if(token_type=='<'){
        match('<');
        return temp<expression();
    }
    else if(token_type==GreatEqual){
        match(GreatEqual);
        return temp>=expression();
    }
    else if(token_type==LessEqual){
        match(LessEqual);
        return temp<=expression();
    }
    else if(token_type==Equal){
        match(Equal);
        return fequal(temp,expression());
    }
    else if(token_type==Nequal){
        match(Nequal);
        return !fequal(temp,expression());
    }
    return temp;
}
void skipbracket(){//����С����
    if(token_type=='(')
        token_type=*pode++;
    int count=0;
    while(token_type&&!(token_type==')'&&count==0)){
        if(token_type==')')count++;//��ʼ����Ƕ��
        if(token_type=='(')count--;
        token_type=*pode++;
    }
    match(')');
}
int boolAND(){
    int val=boolexp();//AND���ȼ�������֮��
    while(token_type==AND){
        match(AND);
        if(val==0)//��һ�����ʽΪ0ֱ�ӷ���0
            return 0;
        val=val&boolexp();
        if(val==0)
            return 0;
    }
    return val;
}
int boolOR(){
    int val=boolAND();
    while(token_type==OR){
        match(OR);
        if(val==1){
            return 1;//��һ�����ʽΪ1ֱ�ӷ���1
        }
        val=val|boolAND();
    }
    return val;
}
void skipbrace(){//����������
    if(token_type=='{')
        token_type=*pode++;
    int count=0;
    while(token_type&&!(token_type=='}'&&count==0)){
        if(token_type=='}')count++;//��ʼ����Ƕ��
        if(token_type=='{')count--;
        token_type=*pode++;
    }
    match('}');//ע�����ﴦ�����},���ǵ���skipbrace�Ĳ��ֲ�Ҫ�����}
}
double statement(){//ִ��һ������
    if(token_type=='{'){//��Ȼ�Ǵ�����,Ҫô�Ǻ���Ҫô��if while
        match('{');
        while(token_type!='}'){
            if(fequal(RETURNFLAG,statement()))//�ݹ�ִ�д������ڲ�����
                return RETURNFLAG;
        }
        match('}');//����������û�з���ֵ
    }
    else if(token_type==If){
        match(If);
        match('(');//�ڴ����Ų��������ڲ�����Ϊ��������
        int boolresult=boolOR();
        while(token_type!='{'&&token_type!=End)//������������
            next_token();
        //match(')');
        if(boolresult){//ifΪ�����ִ�к�������ŵĴ���,��������else��Ҫ����������
            if(fequal(RETURNFLAG,statement()))//ִ�д���
                return RETURNFLAG;//���з���ֵ˵���Ǻ���
        }
        else skipbrace();//ifΪ�ٲ�ִ�д�����
        if(token_type==Else){
            match(Else);
            if(!boolresult){
                if(fequal(RETURNFLAG,statement()))
                    return RETURNFLAG;
            }
            else skipbrace();//if����Ĵ�����ֻ��ִ��һ��
        }
    }
    else if(token_type==While){
        match(While);
        char* while_start=pode;//while���������()��ΪǱ�ڵ�ѭ����ʼλ��
        int boolresult;
        do {
            pode=while_start;//�ص���ʼ�����ж�while����
            token_type='(';//����ִ����������ٻص���ʼ
            match('(');//�ж�while�Ƿ�Ϊ1
            boolresult=boolOR();
            match(')');//����token_typeΪ{
            if(boolresult){
                if(fequal(RETURNFLAG,statement()))//ִ�д����Ŵ���
                    return RETURNFLAG;//����������ֱֵ�ӷ���
            }
            else skipbrace();//��ִ�д�����
        }while(boolresult);
    }
    else if(token_type==Sym||token_type==ArraySym){//�ַ��� ���� ���� ����
        symbol* s=token_sym;//abc = 123; or abc++;
        int token=token_type;
        int index;
        match(token);
        if(token==ArraySym){//����δ�ڱ��ʽ����˵���Ǹ�ֵ
            if(token_type=='='){//һ������ֵ
                int i=0;
                match('=');
                while(token_type!=';'){
                    double t=expression();
                    if(i>=s->value)
                        break;
                    s->list[i]=t;
                    i++;
                    if(token_type==',')
                        match(',');
                }
            }else{
                match('[');
                index=expression();
                match(']');
                match('=');
                if(index>=0&&index<s->value)
                    s->list[index]=expression();
            }
        }else if(token_type=='.'){//�ı��������� ��ʵ��ֵ
            match('.');
            if(token_type==Car){
                match(Car);
                match('=');
                s->car=(symbol*)(int)expression();//��ֵΪ��ַ
            }
            else if(token_type==Cdr){
                match(Cdr);
                match('=');
                s->cdr=(symbol*)(int)expression();//��ֵΪ��ַ
            }
            else{
                match(Val);
                match('=');
                s->value=expression();//��ֵΪʵֵ
            }
        }else{
            if(token_type=='='){//��ֵ �ַ��� �б� ��ֵ
                match('=');
                if(token_type==Str){//�ַ�����ʼ��
                    s->funcp=token_str;//���ַ�����ʶ��ӳ��Ϊָ���ַ�����ʼ��ַ��ָ��
                    s->type=Str;
                    match(Str);
//                }else if(token_type==ConsSym){//�����б�
//                    s->type=ConsSym;
//                    s->value=(int)token_sym;
                }else{//��������ֵ��ʶ����ֵ
                    s->value=expression();//�������ֺ���ı��ʽ
                    s->type=Num;
                }
            }else{//������ֵ�������Լ�
                if(token_type==Inc){
                    match(Inc);
                    s->value+=1.0f;//�������ֺ���ı��ʽ
                }else{
                    match(Dec);
                    s->value-=1.0f;
                }
            }
        }
        match(';');
    }
//    else if(token_type==ConsSym){//�б��ʵ����ָ���ʶ����ָ��
//        symbol* s=token_sym;
//        match(ConsSym);
//        if(token_type=='.'){
//            match('.');
//            if(token_type==Car){//��������ֵ
//                match(Car);
//                match('=');
//                s->funcp=(char*)token_sym;//����һ����ʶ��
//                if(token_type==Sym) match(Sym);
//                else if(token_type==Str) match(Str);
//                else if(token_type==ConsSym) match(ConsSym);
//            }else if(token_type==Cdr){
//                match(Cdr);
//                match('=');
//                s->list=(double*)token_sym;
//                match(Sym);
//            }
//        }else if(token_type=='='){
//            match('=');
//            //�����������б�
//            symbol* t=token_sym;
//            match(ConsSym);
//            s->funcp=t->funcp;
//            s->list=t->list;
//        }
//        match(';');
//    }
    else if(token_type==Func){//������ʶ�� func fun{}
        match(Func);
        match(Sym);
        symbol* s=token_sym;//��ʵ�Ǻ�������
        s->type=FuncSym;
        s->funcp=pode;//�������������ʼλ�ñ�������,pode�ǵ�һ��������λ��
        match('(');
        skipbracket();
        s->value=token_type;//�����ֵ��{
        skipbrace();//δ���ú��������ȷ���
    }
//    else if(token_type==Cons){//�����б�
//        match(Cons);
//        match(Sym);
//        symbol* s=token_sym;
//        s->type=ConsSym;//�����б����
//        s->list=NULL;
//        s->funcp=NULL;
//        s->value=(int)s;//������Ǹ��б�ı�ʶ����ַ�����������
//        match(';');
//    }
    else if(token_type==Array){//��������array mat(10)
        match(Array);
        symbol* s=token_sym;
        match(Sym);
        match('(');
        int length=(int)expression();//�����С
        match(')');
        s->list=(double*)data+data_num;//ָ�����
        s->value=length;
        s->type=ArraySym;
        data_num+=length*sizeof(double);
        match(';');
    }
    else if(token_type==Return){//�������� return(5)
        match(Return);
        match('(');
        return_value=expression();//���������ڵ�ֵ
        match(')');
        match(';');
        return RETURNFLAG;
    }else if(token_type==Winw){//���Ĵ��ڴ�С
        int tmp=0;
        match(Winw);
        match('=');
        tmp=expression();//�����µĴ���ֵ
        match(';');
        if(tmp==draw_w)//�������ڳߴ�ı���޸Ĵ��ڴ�С
            return 0;
        if(tmp>=200&&tmp<=1920){
            draw_w=tmp;
            RECT exclude = {}, include = {};//Ư������
            GetClientRect(draw_handle,&exclude);//��ȡ�����߼��ߴ�,�����߼�����
            GetWindowRect(draw_handle,&include);//��ȡ����ʵ����������������,�����ǿͻ�������Ӱ
            int width = include.right-include.left-exclude.right;//Ư����
            int height = include.bottom-include.top-exclude.bottom;
            SetWindowPos(draw_handle, NULL, include.left, include.top, width+draw_w, height+draw_h, SWP_SHOWWINDOW);//������ʵ�ʳߴ�
        }
    }else if(token_type==Winh){//���Ĵ��ڴ�С
        int tmp=0;
        match(Winh);
        match('=');
        tmp=expression();//�����µĴ���ֵ
        match(';');
        if(tmp==draw_h)//�������ڳߴ�ı���޸Ĵ��ڴ�С
            return 0;
        if(tmp>=100&&tmp<=1080){
            draw_h=tmp;
            RECT exclude = {}, include = {};//Ư������
            GetClientRect(draw_handle,&exclude);//��ȡ�����߼��ߴ�,�����߼�����
            GetWindowRect(draw_handle,&include);//��ȡ����ʵ����������������,�����ǿͻ�������Ӱ
            int width = include.right-include.left-exclude.right;//Ư����
            int height = include.bottom-include.top-exclude.bottom;
            SetWindowPos(draw_handle, NULL, include.left, include.top, width+draw_w, height+draw_h, SWP_SHOWWINDOW);//������ʵ�ʳߴ�
        }
    }else if(token_type==Text){//�Դ�����
        int old_num=data_num;//text�������ʱ�ַ����������ñ������б�����
        char str[DATA_SIZE/2]="";
        double temp;
        match(Text);
        match('(');
        while(token_type!=';'){//ֻҪû����text��������
            if(token_type==Str){//������� (����ֵ,�ַ���,��ʶ��ֵ,��ֵ)
                sprintf(str,"%s%s",str,token_str);
                match(Str);
            }else{
                temp=expression();
                if(fequal(temp,(long long)temp))//������
                    sprintf(str,"%s%I64d",str,(long long)temp);
                else
                    sprintf(str,"%s%.2lf",str,temp);
            }
            if(token_type==',')//���滹��Ҫ��ӡ��
                match(',');
            else//����ֱ�ӱ���
                match(')');
        }
        draw_text(plotter_x,plotter_y,plotter_font,((plotter_color>>16)|((plotter_color&0x000000ff)<<16)|(plotter_color&0x0000ff00)),str,1);
        match(';');
        data_num=old_num;
    }else if(token_type==Pixel){//�Դ�����
        int x,y,c;
        match(Pixel);
        match('(');
        y=(int)expression();//������
        match(',');
        x=(int)expression();//������
        match(',');
        c=(int)expression();//��ɫ
        draw_pixel(x,y,c,1);
        match(')');
        match(';');
    }else if(token_type==Color){//�Դ�����
        match(Color);//������ʾ��ɫ
        match('(');
        plotter_color=(int)expression();
        match(')');
        match(';');
    }else if(token_type==Move){//�Դ�����
        match(Move);//������ʼ��
        match('(');
        plotter_x=(int)expression();
        match(',');
        plotter_y=(int)expression();
        match(')');
        match(';');

    }else if(token_type==Line){//�Դ�����
        int x,y;
        match(Line);
        match('(');
        x=(int)expression();
        match(',');
        y=(int)expression();
        draw_line(plotter_x,plotter_y,x,y,plotter_color,1);
        match(')');
        match(';');
        plotter_x=x;
        plotter_y=y;
    }else if(token_type==Rect){//�Դ�����
        int x1,y1,x2,y2;
        match(Rect);
        match('(');
        x1=(int)expression();
        match(',');
        y1=(int)expression();
        match(',');
        x2=(int)expression();
        match(',');
        y2=(int)expression();
        draw_rect(x1,y1,x2,y2,plotter_color,1);//������
        match(')');
        match(';');
    }else if(token_type==Circle){//�Դ�����
        int x,y,r;
        match(Circle);
        match('(');
        x=(int)expression();
        match(',');
        y=(int)expression();
        match(',');
        r=(int)expression();
        draw_circle(x,y,r,plotter_color,1);
        match(')');
        match(';');
    }else if(token_type==Font){//�Դ�����
        int h;
        match(Font);//�ı�����߶�
        match('(');
        h=(int)expression();
        plotter_font=h;
        match(')');
        match(';');
    }else if(token_type==Flush){//�Դ�����
        match(Flush);//ǿ��ˢ�»�ͼ
        match('(');
        match(')');
        match(';');
        HDC hDC = GetDC(draw_handle);//ˢ�»�ͼ����
        BitBlt(hDC, 0, 0, draw_w, draw_h, draw_dc, 0, 0, SRCCOPY);
        ReleaseDC(draw_handle, hDC);
        memset(draw_fb, 0, WINDOW_W * WINDOW_H * 4);
        draw_fps=get_fps();//��һ�λ��͸���һ��fps
        wrong=0;//�������û����
    }else if(token_type==Graph){//���ƺ���ͼ��
        int dimension=0;//��Ҫ����άͼ����
        context start;//graph������ʼλ��
        int input_x=0;//��������ڷ��ű��λ��
        int input_y=0;
        curse_depth++;
        match(Graph);
        match('(');
        if(token_type!=')'){
            if(token_type==Sym)//��ȡ����x
                input_x=symbol_num-1;
            match(Sym);
            dimension++;
            if(token_type==','){
                match(',');
                if(token_type==Sym)//��ȡ����y
                    input_y=symbol_num-1;
                match(Sym);
                dimension++;
            }
        }
        match(')');
        start=(context){token_type,pode};//ָ��{��ʼ��λ��
        match('{');
        if(dimension==1){//���Ժ���ͼ��
            int y[WINDOW_W];//��ʱ������
            for(int i=0;i<draw_w;i++){
                double cx,cy;//ʵ������
                int wx,wy;//��������
                win_coor(i,0.0,&cx,&cy);
                symbol_list[input_x].value=cx;
                token_type=start.token;
                pode=start.locat;
                statement();
                if(i==mouse_x)
                    coord_value=return_value;//���λ�õ�ֵ
                coor_win(cx,return_value,&wx,&wy);
                y[i]=wy;
            }
            for(int i=1;i<draw_w;i++){//��ͼ�񻭳���
                draw_line(i-1,y[i-1],i,y[i],plotter_color,1);
            }
        }else if(dimension==2){//��������ͼ��
            int color;
            if(graph_slow){//�ҽ����Ϊ�ӳ���Ⱦ
                static unsigned int graph_buffer[WINDOW_H][WINDOW_W]={0};
                static int i=0;
                for(int j=0;j<draw_h;j++){
                    double cx,cy;//ʵ������
                    win_coor(i,j,&cx,&cy);
                    symbol_list[input_x].value=cx;
                    symbol_list[input_y].value=cy;
                    token_type=start.token;
                    pode=start.locat;
                    statement();
                    if(i==mouse_x&&j==mouse_y)
                        coord_value=return_value;//���λ�õ�ֵ
                    int R=plotter_color>>16;//��ɫ
                    int G=(plotter_color&0x0000ff00)>>8;
                    int B=plotter_color&0x000000ff;
                    if(return_value>=1.0)
                        color=plotter_color;//��ɫ
                    else if(return_value<=0.0)
                        color=CRGB(255-R,255-G,255-B);//��ɫ
                    else
                        color=CRGB(255-R+(int)((2*R-255)*return_value),
                                   255-G+(int)((2*G-255)*return_value),
                                   255-B+(int)((2*B-255)*return_value));
                    graph_buffer[j][i]=color;
                }
                i=draw_w-i-1;
                if(i<0||i>=draw_w) i=draw_w-1;//i��ֵ������Ϊǰ��ִ�е��жϳ����쳣
                for(int j=0;j<draw_h;j++){
                    double cx,cy;//ʵ������
                    win_coor(i,j,&cx,&cy);
                    symbol_list[input_x].value=cx;
                    symbol_list[input_y].value=cy;
                    token_type=start.token;
                    pode=start.locat;
                    statement();
                    if(i==mouse_x&&j==mouse_y)
                        coord_value=return_value;//���λ�õ�ֵ
                    int R=plotter_color>>16;//��ɫ
                    int G=(plotter_color&0x0000ff00)>>8;
                    int B=plotter_color&0x000000ff;
                    if(return_value>=1.0)
                        color=plotter_color;//��ɫ
                    else if(return_value<=0.0)
                        color=CRGB(255-R,255-G,255-B);//��ɫ
                    else
                        color=CRGB(255-R+(int)((2*R-255)*return_value),
                                   255-G+(int)((2*G-255)*return_value),
                                   255-B+(int)((2*B-255)*return_value));
                    graph_buffer[j][i]=color;
                }
                i=draw_w-i-1;
                i++;
                if(i==draw_w/2)
                    i=0;
                memcpy(draw_fb,graph_buffer,WINDOW_H*WINDOW_W*sizeof(unsigned int));//���Ƶ���Ļ����
            }else{
                for(int i=0;i<draw_w;i++)
                    for(int j=0;j<draw_h;j++){
                        double cx,cy;//ʵ������
                        win_coor(i,j,&cx,&cy);
                        symbol_list[input_x].value=cx;
                        symbol_list[input_y].value=cy;
                        token_type=start.token;
                        pode=start.locat;
                        statement();
                        if(i==mouse_x&&j==mouse_y)
                            coord_value=return_value;//���λ�õ�ֵ
                        int R=plotter_color>>16;//��ɫ
                        int G=(plotter_color&0x0000ff00)>>8;
                        int B=plotter_color&0x000000ff;
                        if(return_value>=1.0)
                            color=plotter_color;//��ɫ
                        else if(return_value<=0.0)
                            color=CRGB(255-R,255-G,255-B);//��ɫ
                        else
                            color=CRGB(255-R+(int)((2*R-255)*return_value),
                                       255-G+(int)((2*G-255)*return_value),
                                       255-B+(int)((2*B-255)*return_value));
                        draw_pixel(j,i,color,1);
                    }
            }
        }
        #if 1
        else if(dimension==0){//���ڲ���
            static double x1=0;
            x1=sin(cycle/1000);
            for(int i=0;i<draw_w;i++)
                for(int j=0;j<draw_h;j++){
                    double x,y;//ʵ������
                    win_coor(i,j,&x,&y);
                    double yy = y*y;
                    double d0 = sqrt(x*x+yy);
                    double d1 = sqrt((x+x1)*(x+x1)+yy);
                    double d = cycle*0.01;
                    d0 = sin((d0-d)*16);
                    d1 = sin((d1-d)*16);
                    int g = (d0+d1)*64+128;
                    int r = d0*64+128;
                    int b = d1*64+128;
                    draw_pixel(j,i,CRGB(r,g,b),1);
            }
        }
        #endif // 0
        #if 0
        else if(dimension==0){//���ڲ���
            int r=255,g=255,b=0,col;
            for(int i=0;i<draw_w;i++)
                for(int j=0;j<draw_h;j++){
                    double x,y;//ʵ������
                    win_coor(i,j,&x,&y);
                    double c = 1, ox = x, oy = y;
                    while(c>0){
                        double xx = x*x, yy = y*y;
                        if (xx+yy >= 4.0) break;
                        y = x*y*2+oy;
                        x = xx-yy+ox;
                        c = c-1.0/32.0;
                    }
                    if(c>=1.0)
                        col=CRGB(r,g,b);//��ɫ
                    else if(c<=0.0)
                        col=CRGB(255-r,255-g,255-b);//��ɫ
                    else
                        col=CRGB(255-r+(int)((2*r-255)*c),
                                 255-g+(int)((2*g-255)*c),
                                 255-b+(int)((2*b-255)*c));
                    draw_pixel(j,i,col,1);
            }
        }
        #endif // 0
        token_type=start.token;
        pode=start.locat;
        skipbrace();//����������
        while(symbol_list[symbol_num-1].depth==curse_depth)//���ú������ٵ����ڴ�Ҫ���
            symbol_num--;
        curse_depth--;
    }else if(token_type==Show){//��������ϵ������괦��������ϵ����
        double x,y;
        int px=plotter_x,py=plotter_y;//��ϣ���ı�plotter������
        char str[20]="";
        match(Show);
        match('(');
        match(')');
        match(';');
        draw_hor_line(origin_y,0,draw_w-1,plotter_color,1);//������
        draw_ver_line(origin_x,0,draw_h-1,plotter_color,1);
        win_coor(mouse_x,mouse_y,&x,&y);
        sprintf(str,"x:%.4lf\ny:%.4lf\nv:%.4lf",x,y,coord_value);
        draw_text(mouse_x,mouse_y,plotter_font,((plotter_color>>16)|((plotter_color&0x000000ff)<<16)|(plotter_color&0x0000ff00)),str,1);
        plotter_x=px;
        plotter_y=py;
    }else if(token_type==Slow){//�ı��ͼģʽ
        match(Slow);
        match('(');
        graph_slow=(int)expression();
        match(')');
        match(';');
    }else{
        factor();//�����˺��� ���Ҽ�ʹ�����з���ֵҲû�лش���ֵ ��˽�����һ����ƥ��������
        match(';');//�������ֻҪ��ƥ��һ����ʾ����������
    }
    return 0;
}
double function(){//ִ��һ�����������غ���ֵ ����˼���ǣ�����c���Ա����ǵݹ���õģ�����Զ��庯������Ҳ�ǵݹ���õ�
    if(curse_depth>CURSE_DEEP)
        match(End);//�������ݹ����
    return_value=0.0f;
    symbol* s=token_sym;//��������ı�ʶ��
    match(FuncSym);
    match('(');
    //�Ƚ���������ֵȫ�����
    double parameter[PARAMETER_SIZE]={0.0};
    int para=0;//��������
    while(token_type!=')'&&token_type!=End){
        if(para==PARAMETER_SIZE)
            match(End);//���ι���
        parameter[para]=expression();
        para++;
        if(token_type==',')
            match(',');
    }
    match(')');
    context cont_call={token_type,pode};//���浱ǰ���ú�����Դ����λ���������Ա���淵��
    pode=s->funcp;//PC�����������ת�����������λ��,����1��������λ��
    token_type='(';
    match('(');
    curse_depth++;//Ƕ�ײ���++ ��Ժ���������������
    int pa=0;
    while(token_type!=')'&&token_type!=End){//�����������еı�������������˳����뺯��������
        if(pa==PARAMETER_SIZE)
            match(End);//���ι���
        symbol_list[symbol_num]=*token_sym;//����������ʱ�Ĳ�����ʶ�����뺯��������
        strcpy(symbol_list[symbol_num].name,token_sym->name);
        symbol_list[symbol_num].depth=curse_depth;
        symbol_list[symbol_num].value=parameter[pa];//�������嶨���ʶ����ֵΪ��������ֵ,�������������Ǳ�ʶ���������з���ֵ����ֵ
        symbol_num++;
        pa++;//�����������򱨴�,������С��0
        match(Sym);
        if(token_type==',')
            match(',');//���ϲ��뺯��������Ĳ���
    }
    if(pa<para)
        match(End);//���ι���
    match(')');
    token_type=(int)s->value;
    statement();//ִ�к����˹�
    pode=cont_call.locat;//������
    token_type=cont_call.token;//�ָ����Է�
    while(symbol_list[symbol_num-1].depth==curse_depth)//���ú������ٵ����ڴ�Ҫ���
        symbol_num--;
    curse_depth--;
    return return_value;
}
/****************************��Ϣ����****************************///��ʱ����
typedef struct{
    char name[32][32];//���֧��32�������
    unsigned int data[32];//������Ϣ
    unsigned char top;//ʵʱ���µ�ջ���Ƚ������ٵĶ���
    unsigned char num;
}Message;
Message msg_console=(Message){{{0}},{0},0,0};
void msg_reset(Message *msg){
    msg->top=msg->num=0;
}
void msg_add(Message *msg,char *str,unsigned int extra){
    strcpy(msg->name[msg->top],str);
    msg->data[msg->top]=extra;
    msg->num=msg->num==32?32:msg->num+1;
    msg->top=(msg->top+1)%32;//top�ȴ���
}
int msg_get(Message *msg,int i){//��ȡ��i����Ϣ��ʵ��λ�ã����ڱ���
    return (msg->top-msg->num+32+i)%32;
}
/****************************������Ϣ****************************/
int get_line(){//��ȡ���е���괦���ַ���,,
    int tmp_c=cursor-1,line=0;
    while(tmp_c>=0&&code[tmp_c]!='\n'){
        line++;
        tmp_c--;
    }
    return line;
}
void get_code(char c){//�ڹ�괦����һ���ַ���ɾ���ַ�
    if(c==0){//����ɾ
        for(int i=cursor-1;i<node;i++)
            code[i]=code[i+1];
        cursor--;
        node--;
        return;
    }
    if(c==-1){//����ɾ
        for(int i=cursor;i<node;i++)
            code[i]=code[i+1];
        node--;
        return;
    }
    for(int i=node;i>cursor;i--)
        code[i]=code[i-1];
    code[cursor]=c;
    cursor++;
    cursor_c++;
    node++;
    if(c=='\n'){
        cursor_c=0;
        cursor_r++;
    }
}
int get_fps(){//ms
//    static int fps = 0;
//    static int lastTime = 0;//= clock(); // ms
//    static int frameCount = 0;
//    frameCount++;
//    int curTime = clock();
//    if (curTime - lastTime > 1000){ // ȡ�̶�ʱ����Ϊ1��
//        fps = frameCount;//��Ϊ��0.5��ˢ��һ��Ҫ��2
//        frameCount = 0;
//        lastTime = curTime;
//    }
//    return fps;
    static int lastTime = 0;
    int curTime=clock();
    int fps=curTime-lastTime;
    lastTime=curTime;
    return fps;
}
void draw_message(){
    char s[50]="";
    sprintf(s,"FPS:%2dms Row:%d Col:%d", draw_fps,cursor_r,cursor_c);
    draw_text(0,0,0,RGB(255,127,39),s,0);
}
char *key_word(int n){//���Է��������ַ���ת����
    switch(n){
        case End:return "end";
        case Num:return "num";
        case Str:return "str";
        case Array:return "array";
        case Func:return "func";
        //case Cons:return "cons";
        case Car:return "car";
        case Cdr:return "cdr";
        case Val:return "val";
        case Else:return "else";
        case If:return "if";
        case Return:return "return";
        case While:return "while";
        case Text:return "text";
        case Read:return "read";
        case Sqrt:return "sqrt";
        case Cos:return "cos";
        case Sin:return "sin";
        case Abs:return "abs";
        case Tan:return "tan";
        case Log:return "log";
        case Floor:return "flor";
        case Pi:return "PI";
        case E:return "E";
        case Clock:return "clock";
        case Mousx:return "mousx";
        case Mousy:return "mousy";
        case Winw:return "winw";
        case Winh:return "winh";
        case Rand:return "rand";
        case Color:return "color";
        case Pixel:return "pixel";
        case Rgb:return "rgb";
        case Move:return "move";
        case Line:return "line";
        case Rect:return "rect";
        case Circle:return "circle";
        case Flush:return "flush";
        case Font:return "font";
        case Graph:return "graph";
        case Show:return "show";
        case Slow:return "slow";
        case Assign:return "assign";
        case OR:return "or";
        case AND:return "and";
        case Equal:return "equal";
        case Sym:return "sym";
        case FuncSym:return "funcsym";
        case ArraySym:return "arraysym";
        //case ConsSym:return "conssym";
        case Void:return "void";
        case Nequal:return "nequal";
        case LessEqual:return "lessEqual";
        case GreatEqual:return "greatEqual";
        case Inc:return "inc";
        case Dec:return "dec";
    }
    return NULL;
}
//��������ؼ�������ɫ
//ע�� ���� �ؼ��� ���� �ַ� ����
unsigned int key_color[]={RGB(200,191,231),RGB(255,242,0),RGB(0,162,232),RGB(199,71,199),RGB(255,127,39),RGB(181,230,29)};
void draw_segment(char *s,int x,int y){//����һ���ν�����ӡ����
    //ע�� ���� �ؼ��� ���� �ַ� ����
    if(*s=='#')
        draw_text(x,y,18,key_color[0],s,0);//ע��
    else if(*s>='0'&&*s<='9')
        draw_text(x,y,18,key_color[5],s,0);//����
    else if(*s<'A'||(*s>'Z'&&*s<'a')||*s>'z')
        draw_text(x,y,18,key_color[4],s,0);//�ַ�
    else{
        int i=0;
        for(i=Array;i<Assign;i++)//��������ķ�Χ
            if(strcmp(s,key_word(i))==0)
                break;
        if(i==Assign)
            draw_text(x,y,18,0x00ffffff,s,0);//��ͨ��ʶ��
        else if(i<Else)
            draw_text(x,y,18,key_color[1],s,0);//����
        else if(i<Text)
            draw_text(x,y,18,key_color[2],s,0);//�ؼ���
        else
            draw_text(x,y,18,key_color[3],s,0);//����
    }
}
//������ʾ
void draw_code(){
    //����������
    for(int i=line_start+1,ls=18;i<=line_start+25;i++,ls+=18){
        char s[4]="";
        sprintf(s,"%d",i);
        draw_text(0,ls,0,RGB(34,177,76),s,0);
    }
    //������ִ�г���,���������Ϣ,��ִ�г����λ�ñ��
    if(wrong){
        /**���ڿ��µĶ��߳�,ǧ��Ҫ�����޸Ķ��̹߳����ȫ�ֱ���**/
        int row=(wrong_row-line_start)*18;//ȥ��У׼��
        int col=wrong_col*8;
        char wrong_code[32]="Wrong:";//���������������Ϣ
        if(wrong_type<128)//��⵽
            sprintf(wrong_code,"%s%c ",wrong_code,(char)wrong_type);
        else
            sprintf(wrong_code,"%s%-6s ",wrong_code,key_word(wrong_type));
        if(wrong<128)//����ֵ
            sprintf(wrong_code,"%sRight:%c ",wrong_code,(char)wrong);
        else
            sprintf(wrong_code,"%sRight:%-6s ",wrong_code,key_word(wrong_type));
        draw_rect(col+20,row,screen_w-30,row+18,CRGB(255,0,0),0);
        draw_text(180,0,0,RGB(34,197,76),wrong_code,0);
    }
    //�����
    if((cursor_r-line_start+2)*18>screen_h)//���������ȥ��
        line_start++;
    else if(cursor_r<line_start)
        line_start--;
    draw_hor_line((cursor_r-line_start+2)*18,cursor_c*8+20,cursor_c*8+27,0x00ffffff,0);
    //������
    char*p=code;//ָ��ǰ����Ĵ���
    char str[DATA_SIZE/2]="";//����������ʾ���ַ���
    int x=0,y=18,len,tmp=0;//Ӧ����ʾ�Ĵ�����ʼ����
    if(line_start)//���滹�д���
        draw_text(screen_w-30,18,30,RGB(255,201,14),"��",0);
    while(tmp<line_start)//����ʼ�п�ʼ��ʾ
        if(*p++=='\n')
            tmp++;
    while(*p){
        len=0;
        if((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')){
            while((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z'))
                str[len++]=*p++;
        }
        else if(*p=='\n'){
            x=0;
            y+=18;
            p++;
            if(y+18>screen_h){//������Ļ��ʾ��Χ��
                draw_text(screen_w-30,screen_h-30,30,RGB(255,201,14),"��",0);
                break;
            }
            continue;
        }
        else if(*p=='#'){
            while(*p&&*p!='\n')
                str[len++]=*p++;
        }
        else if(*p>='0'&&*p<='9'){
            while(*p>='0'&&*p<='9')
                str[len++]=*p++;
        }else{
            str[0]=*p++;
            len=1;
        }
        str[len]=0;
        draw_segment(str,x+20,y);
        x+=len*8;
    }
    //�������ǰ������ִ����һ��
    int row,col;
    get_posion(rode,&row,&col);
    draw_text(0,(row-line_start)*18-8,30,RGB(255,201,14),"��",0);
}
/*******the*hitchhikers*guide*to*the*galaxy*******/
void car_init(int wx,int wy,char *s){
    win32_init(wx,wy,s);
    node=strlen(code);
    srand(time(NULL));
}

void car_con(){
    //����
    if(is_press && press_time<20){
        press_time++;
        return;//ֻ����һ�λ򴥷��ܶ��
    }
    int shift=0;//��Сд
    if (screen_keys[VK_SHIFT]) shift=1;
    if (screen_exit != 0 || screen_keys[VK_ESCAPE] != 0) car_run=0;
    if (screen_keys[VK_UP]){
        if(cursor_r>0){
            int line_1=0;
            int line_2=0;
            cursor_r--;
            cursor--;
            while(code[cursor]!='\n'){
                cursor--;
                line_2++;
            }
            cursor--;
            while(cursor>=0&&code[cursor]!='\n'){
                cursor--;
                line_1++;
            }
            if(line_1<=line_2){
                cursor+=line_1+1;
                cursor_c=line_1;
            }
            else
                cursor+=line_2+1;
        }else
            cursor=cursor_c=cursor_r=0;
        is_press=1;
    }
    if (screen_keys[VK_DOWN]){
        int line_1=0;
        int line_2=0;
        cursor--;
        while(cursor>=0&&code[cursor]!='\n'){
            cursor--;
            line_1++;
        }
        cursor++;
        cursor_c=0;
        while(code[cursor]&&code[cursor]!='\n'){
            cursor++;
            cursor_c++;
        }
        if(code[cursor]!=0){
            cursor_r++;
            cursor++;
            while(code[cursor]&&code[cursor]!='\n'){
                cursor++;
                line_2++;
            }
            if(line_1>=line_2)
                cursor_c=line_2;
            else{
                cursor-=line_2-line_1;
                cursor_c=line_1;
            }
        }
        is_press=1;
    }
    if (screen_keys[VK_LEFT]){
        cursor--;
        if(code[cursor]=='\n'){
            int line=0;
            cursor--;
            cursor_r--;
            while(cursor>=0&&code[cursor]!='\n'){
                cursor--;
                line++;
            }
            cursor+=line+1;
            cursor_c=line;
        }else
            cursor_c--;
        if(cursor<0)
            cursor_c=cursor_r=cursor=0;
        is_press=1;
    }
    if (screen_keys[VK_RIGHT]){
        if(cursor<node){
            cursor_c++;
            if(code[cursor]=='\n'){
                cursor_c=0;
                cursor_r++;
            }
            cursor++;
        }
        is_press=1;
    }
    if (screen_keys[KEY_ENTER]){
        //����ǰ�еĿո����Ա����
        char *tmp=code+cursor-1;
        int space=0;
        while(tmp>=code&&*tmp!='\n'){
            if(*tmp==' ')
                space++;
            else
                space=0;
            tmp--;
        }
        get_code('\n');
        while(space--)
            get_code(' ');
        is_press=1;
    }
    if (screen_keys[KEY_BAK]){
        if(cursor>0){
            if(code[cursor-1]=='\n')
                cursor_r--;
            if(cursor>2&&code[cursor-1]==' '&&code[cursor-2]==' '&&code[cursor-3]==' '){//ȥ��tab��
                get_code(0);
                get_code(0);
            }
            get_code(0);
            cursor_c=get_line();
        }
        is_press=1;
    }
    if (screen_keys[KEY_DEL]){
        if(cursor<node)
            get_code(-1);
        is_press=1;
    }
    if(!screen_keys[VK_CONTROL])//�������ʱ���ܰ�������
        for(int key=KEY_A;key<=KEY_Z;key++)
            if(screen_keys[key]){
                get_code(shift?key:key-'A'+'a');
                is_press=1;
            }
    if (screen_keys[VK_SHIFT]){
        if(screen_keys[KEY_3]){
            get_code('#');
            is_press=1;
        }
        if(screen_keys[KEY_GRE]){
            get_code('>');
            is_press=1;
        }
        if(screen_keys[KEY_LES]){
            get_code('<');
            is_press=1;
        }
        if(screen_keys[KEY_PLU]){
            get_code('+');
            is_press=1;
        }
        if(screen_keys[KEY_9]){
            get_code('(');
            is_press=1;
        }
        if(screen_keys[KEY_5]){
            get_code('%');
            is_press=1;
        }
        if(screen_keys[KEY_8]){
            get_code('*');
            is_press=1;
        }
        if(screen_keys[KEY_0]){
            get_code(')');
            is_press=1;
        }
        if(screen_keys[KEY_6]){
            get_code('^');
            is_press=1;
        }
        if(screen_keys[KEY_7]){
            get_code('&');
            is_press=1;
        }
        if(screen_keys[KEY_1]){
            get_code('!');
            is_press=1;
        }
        if(screen_keys[KEY_LB]){
            get_code('{');
            is_press=1;
        }
        if(screen_keys[KEY_RB]){
            get_code('}');
            is_press=1;
        }
        if(screen_keys[KEY_SUB]){
            get_code('_');
            is_press=1;
        }
        if(screen_keys[KEY_SLASH]){
            get_code('|');
            is_press=1;
        }
        return;
    }
    if(screen_keys[VK_CONTROL]){
        if(screen_keys[KEY_V]){//ճ��
            //�ȿ��ܷ�򿪼�����
            if(OpenClipboard(screen_handle)){
                //��ȡ����������,��֧��ANSI�ı�
                HANDLE hClipboardData = GetClipboardData(CF_TEXT);
                //ָ��ָ�����������
                char *pchData = (char*)GlobalLock(hClipboardData);
                //����������
                while(*pchData){
                    if(*pchData!='\r')
                        get_code(*pchData);
                    pchData++;
                }
                //����ȫ���ڴ�
                GlobalUnlock(hClipboardData);
                //�رռ������Ա���������ʹ��
                CloseClipboard();
            }
            is_press=1;
        }
        if(screen_keys[KEY_C]){//����ȫ��
            //�ȿ��ܷ�򿪼�����
            if (OpenClipboard(screen_handle)){
                //��ռ�����
                EmptyClipboard();
                //Ϊ����������ڴ�
                HGLOBAL hClipboardData;
                hClipboardData = GlobalAlloc(GMEM_DDESHARE,
                                            node+1);
                //���ָ��������ָ��
                char * pchData;
                pchData = (char*)GlobalLock(hClipboardData);
                //�Ӿֲ��������Ƶ�ȫ���ڴ�
                strcpy(pchData, code);
                //�����ڴ�
                GlobalUnlock(hClipboardData);
                //����������ճ����ȫ���ڴ�
                SetClipboardData(CF_TEXT,hClipboardData);
                //�����������Ա���������ʹ��
                CloseClipboard();
            }
            is_press=1;
        }
        if(screen_keys[KEY_D]){//���
            memset(code,0,node);
            node=cursor=cursor_c=cursor_r=line_start=0;
            is_press=1;
        }
        return;
    }
    for(int key=KEY_0;key<=KEY_9;key++)
        if(screen_keys[key]){
            get_code(key);
            is_press=1;
        }
    if(screen_keys[KEY_PLU]){
        get_code('=');
        is_press=1;
    }
    if(screen_keys[KEY_SUB]){
        get_code('-');
        is_press=1;
    }
    if(screen_keys[KEY_LB]){
        get_code('[');
        is_press=1;
    }
    if(screen_keys[KEY_RB]){
        get_code(']');
        is_press=1;
    }
    if(screen_keys[KEY_SEM]){
        get_code(';');
        is_press=1;
    }
    if(screen_keys[KEY_QUO]){
        get_code('\'');
        is_press=1;
    }
    if(screen_keys[KEY_LES]){
        get_code(',');
        is_press=1;
    }
    if(screen_keys[KEY_GRE]){
        get_code('.');
        is_press=1;
    }
    if(screen_keys[KEY_QUE]){
        get_code('/');
        is_press=1;
    }
    if(screen_keys[VK_SPACE]){
        get_code(' ');
        is_press=1;
    }
    if(screen_keys[KEY_SLASH]){
        get_code('\\');
        is_press=1;
    }
    if(screen_keys[VK_TAB]){
        get_code(' ');
        get_code(' ');
        get_code(' ');
        is_press=1;
    }
    //���
    if(mouse_lclick){
        static int mouse_ox,mouse_oy;
        int mx,my;
        if(!mouse_have_lclick){//�״ΰ��¼�¼�������
            mouse_have_lclick=1;
            mouse_ox=mouse_x;
            mouse_oy=mouse_y;
            return;
        }
        mx=mouse_x-mouse_ox;//��������ƶ�ʸ��
        my=mouse_y-mouse_oy;
        origin_x+=mx;
        origin_y+=my;
        mouse_ox=mouse_x;
        mouse_oy=mouse_y;
    }
    if(mouse_rclick){
        static int mouse_oy;
        int my;
        if(!mouse_have_rclick){//�״ΰ��¼�¼�������
            mouse_have_rclick=1;
            mouse_oy=mouse_y;
            return;
        }
        my=mouse_y-mouse_oy;//��������ƶ�ʸ��
        double delta_ratio=log(coord_ratio+1.0)*my/42.0;//coord_ratio/128.0*my; ���ű����ı���
        double old_ratio=coord_ratio;//�����ű���
        coord_ratio=coord_ratio+delta_ratio;//����Խ��,����Խ��,����ԽС,����Խ��
        origin_x=draw_w/2.0+(origin_x-draw_w/2.0)*old_ratio/coord_ratio;//����ԭ��ͬ����Ҫ����λ�÷����ı�
        origin_y=draw_h/2.0+(origin_y-draw_h/2.0)*old_ratio/coord_ratio;
        mouse_oy=mouse_y;
    }
}

void *car_upd(){
    pthread_detach(pthread_self());//�߳̽������Զ��ͷ���Դ
    // �����ű�������ӹؼ���
    for(int i=Array;i<Assign;i++){
        find_symbol(key_word(i));//�������ùؼ���token
        symbol_list[symbol_num-1].type=i;//�������Է�Ϊ���ñ�ʶ���Է�
    }
    while(car_run){
        symbol_num=Assign-Array;
        curse_depth=data_num=plotter_x=plotter_y=0;//һ�����¿�ʼ
        plotter_font=18;//Ĭ�������Ժ�
        plotter_color=0x00ff7f27;//Ĭ����ɫ����ɫ
        pode=rode=code;
        //�������ڻ�ͼ������
        next_token();//ֻҪ���˵�һ�����Է�
        if(setjmp(jump) == 0){//����ִ�й���һ��������������
            while (token_type!=End)
                statement();//���ܳ���������������
            wrong=0;/**���ڶ��߳�,���ܼ������ִ�й���û�г���,�������ִ�е��˴����β**/
        }
        //��������������ϲŻ��ƻ�ͼ����,���ߴ����Լ�����ˢ�´���
        //��Ϊ��ͼ��ʱ��Ͳ���ִ�к�����
        HDC hDC = GetDC(draw_handle);//��ͼ����,����ʹ�þֲ�����,���߳�һ��Ҫ�����޸Ĺ���ȫ�ֱ���
        BitBlt(hDC, 0, 0, draw_w, draw_h, draw_dc, 0, 0, SRCCOPY);
        ReleaseDC(draw_handle, hDC);
        memset(draw_fb, 0, WINDOW_W * WINDOW_H * 4);
        draw_fps=get_fps();//���»滭����fps
        cycle=cycle+1.0;//����������
    }
    pthread_exit(0);
    return NULL;
}

void car_dra(){
    //��ʼ��ͼ,ֻ��������
    draw_message();
    draw_code();
    //��ʾ��ͼ��������
    HDC hDC = GetDC(screen_handle);//������
    BitBlt(hDC, 0, 0, screen_w, screen_h, screen_dc, 0, 0, SRCCOPY);
    ReleaseDC(screen_handle, hDC);
    win32_dispatch();//������Ϣ
    //�������
    memset(screen_fb, 0, WINDOW_W * WINDOW_H * 4);
}

void car_close(){
    win32_close();
}

int main(void){
    car_init(screen_w,screen_h,"ISLANDS");
    //��Ը���߳�����ѭ��Ҳ��Ҫ��ѭ���ﴴ�߳�
    pthread_t tid;//Ϊִ�д��벿�ֵ��������߳�
    pthread_create(&tid,NULL,car_upd,NULL);//����ִ��
    while(car_run){
        car_con();//�������
        car_dra();//���ƴ���
        Sleep(frame);//֡����
    }
    car_close();
    return 0;
}
