#include <windows.h>
#include <stdio.h>
#include <pthread.h>//为执行代码单独分配线程
#include <float.h>
#include <setjmp.h>//用于非局部goto处理异常
#include <time.h>
#include <math.h>
//=====================================================================
// Win32 窗口及图形绘制
//=====================================================================
#define WINDOW_W 1920//最大允许的窗口尺寸大小
#define WINDOW_H 1080
int draw_w, draw_h, screen_exit = 0;
const int screen_w,screen_h;
int mouse_x,mouse_y;                    //鼠标坐标
int mouse_lclick=0;                     //鼠标左键是否按下
int mouse_have_lclick=0;                //鼠标左键是否已经按下，执行一次
int mouse_rclick=0;                     //鼠标右键是否按下
int mouse_have_rclick=0;                //鼠标右键是否已经按下，执行一次
int screen_keys[512];	                //当前键盘按下状态
int is_press=0;                         //键盘是否已经按下
int press_time=0;                       //键盘按下时间
static HWND screen_handle = NULL;       //主窗口 HWND
static HDC screen_dc = NULL;            //主窗口内存设备上下文 HDC
static HBITMAP screen_hb = NULL;        //主窗口位图 DIB
unsigned int *screen_fb = NULL;        //主窗口frame buffer
static HWND draw_handle = NULL;         //绘画窗 HWND
static HDC draw_dc = NULL;              //绘画窗 HDC
static HBITMAP draw_hb = NULL;          //绘画窗位图 DIB
unsigned int *draw_fb = NULL;          //绘画窗frame buffer

int win32_init(int w, int h, char *title);	// 屏幕初始化
void win32_close(void);					// 关闭屏幕
void win32_dispatch(void);					// 处理消息
// win32 event handler
static LRESULT win32_events(HWND, UINT, WPARAM, LPARAM);

static LRESULT win32_events(HWND hWnd, UINT uMsg,WPARAM wParam, LPARAM lParam)
{//6.窗口消息的处理程序
	HDC hdc;
    switch (uMsg)
    {
    case WM_CLOSE:
        screen_exit = 1;
        break;
    case WM_LBUTTONDOWN://左键
		mouse_lclick=1;
		break;
	case WM_LBUTTONUP:
		mouse_lclick=0;
        mouse_have_lclick=0;
		break;
    case WM_RBUTTONDOWN://右键
        mouse_rclick=1;
		break;
    case WM_RBUTTONUP:
		mouse_rclick=0;
        mouse_have_rclick=0;
		break;
	case WM_MOUSEMOVE://鼠标移动事件
		hdc=GetDC(hWnd);//初始化HDC
		mouse_x=LOWORD(lParam);//鼠标的横坐标
		mouse_y=HIWORD(lParam);//鼠标的纵坐标
		ReleaseDC(hWnd,hdc);//搞完人走带门，释放HDC
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
        //采用默认方式处理的消息返回非0值
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;//采用显式处理的消息返回0值
}

void win32_dispatch(void)
{//5.进入窗口的消息循环
    MSG msg;//将检索到的消息复制到MSG变量
    while (1){
        //从当前线程消息队列检索第1条消息，复制到msg变量，并从消息队列中删除
        if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
            break;//检测到WM_QUIT消息即退出
        if (!GetMessage(&msg, NULL, 0, 0))//检索线程所有任意编号的消息
            break;
        //检索到正常待处理的消息
        DispatchMessage(&msg);//将消息分发到win32_events()窗口过程进行处理
    }
}
// 初始化窗口并设置标题
int win32_init(int w, int h, char *title)
{
    WNDCLASSA wc = {CS_BYTEALIGNCLIENT, (WNDPROC)win32_events,//绑定窗口的消息处理函数
                    0, 0, 0,NULL, NULL, NULL, NULL, title//窗口类名
                   };//1.创建窗口类
    BITMAPINFO bi = { {
            sizeof(BITMAPINFOHEADER), WINDOW_W, -WINDOW_H, 1, 32, BI_RGB,
            WINDOW_W * WINDOW_H * 4, 0, 0, 0, 0
        },{{0,0,0,0}}
    };
    RECT rect = { 0, 0, w, h };
    int wx, wy, sx, sy;
    LPVOID ptr;

    win32_close();

    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);//窗口背景色
    wc.hInstance = GetModuleHandle(NULL);//窗口类隶属的应用程序句柄
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);//鼠标显示为十字光标
    if (!RegisterClass(&wc)) return -1;//2.注册窗口类

    screen_handle = CreateWindow(title, "EDITOR",//3.用注册的窗口类创建实例窗口
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,//窗口的样式
                                  0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

    draw_handle = CreateWindow(title, "PLOTTER",//创建成功返回窗口句柄
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
    if (screen_handle == NULL) return -2;
    screen_exit = 0;
    HDC hDC;
    /***主窗口绘图缓冲***/
    hDC = GetDC(screen_handle);
    //创建一个与指定设备兼容的内存设备上下文环境DC
    screen_dc = CreateCompatibleDC(hDC);
    ReleaseDC(screen_handle, hDC);
    //创建应用程序可以直接写入的与设备无关的位图DIB
    screen_hb = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
    if (screen_hb == NULL) return -3;
    SelectObject(screen_dc, screen_hb);//将位图数据设置到当前设备里
    screen_fb = (unsigned int*)ptr;
    /***绘画窗绘图缓冲***/
    hDC = GetDC(draw_handle);
    //创建一个与指定设备兼容的内存设备上下文环境DC
    draw_dc = CreateCompatibleDC(hDC);
    ReleaseDC(draw_handle, hDC);
    //创建应用程序可以直接写入的与设备无关的位图DIB
    draw_hb = CreateDIBSection(draw_dc, &bi, DIB_RGB_COLORS, &ptr, 0, 0);
    if (draw_hb == NULL) return -3;
    SelectObject(draw_dc, draw_hb);//将位图数据设置到当前设备里
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

    ShowWindow(screen_handle, SW_NORMAL);//4.显示窗口
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
/******************************全局变量*******************************/
#define PI 3.1415926f
#define BUFFER_SIZE 1024*8  //缓冲区大小
int draw_fps = 0;//绘画窗的fps
const int screen_w = 640;//主窗口尺寸
const int screen_h = 480;
int car_run = 1;//是否在运行
int frame = 1;//帧速率
jmp_buf jump;//设置标号以便遇到语法错误可以跳转
int wrong = 0;//代码执行是否遇到了错误?
int wrong_type = 0;//代码错误点遇到的属性符
int wrong_row,wrong_col;//错误代码的行、列
unsigned int plotter_color=0x00ff7f27;//当前绘图的颜色
int plotter_x=0,plotter_y=0;//当前绘图的起始点
int plotter_font=18;//字体大小
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
int node=0;//代码量
int cursor=0;//光标
int cursor_r=0;//光标行数
int cursor_c=0;//光标列数
int line_start=0;//代码显示起始行
enum {//键码
    KEY_BAK=8,KEY_TAB,KEY_ENTER=13,KEY_DEL=46,KEY_SEM=186,KEY_PLU,KEY_LES,KEY_SUB,KEY_GRE,KEY_QUE,KEY_LB=219,KEY_SLASH,KEY_RB,KEY_QUO,
    KEY_0=48,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,
    KEY_A=65,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z,
    };
/****************************基本函数****************************/
//ABS
#define ABS(x) ((x)<=0?-(x):(x))
//SIGN
#define SIGN(x) (x > 0 ? 1 : (x < 0 ? -1 : 0))
//RGB转r,g,b
#define CRGB(a,b,c) (((a)<<16)|((b)<<8)|(c))
//判断是否在屏幕内部
int in_screen(int,int,int);
inline int in_screen(int a,int b,int w){//w参数指示当前是要在几号窗口画画
    if(w==0)//主窗口
        return ((a)>=0&&(b)>=0&&(a)<screen_w&&(b)<screen_h);
    else//绘图窗
        return ((a)>=0&&(b)>=0&&(a)<draw_w&&(b)<draw_h);
}
//绘制像素到与窗口尺寸相当的屏幕缓冲
//screen_fb[((y)<<9)+((y)<<7)+(x)]=c
void draw_pixel(int,int,unsigned int,int);
inline void draw_pixel(int y,int x,unsigned int c,int w){
    if(in_screen(x,y,w)){
        if(w==0)
            screen_fb[y*WINDOW_W+x]=c;
        else
            draw_fb[y*WINDOW_W+x]=c;//改变窗口尺寸只是在左上角一块小区域绘制
    }
}
//浮点数相等
int fequal(double,double);
inline int fequal(double a,double b){
    return (ABS((a)-(b))<1e-9);
}
int get_fps();
/****************************图像绘制****************************/
int find_region(int x, int y,int win){//Cohen-Sutherland裁线算法
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
//将2D直线裁剪至屏幕空间,9块区域输出码如下:
//9 8 10
//1 0 2
//5 4 6
void clip_line(int x1, int y1, int x2, int y2, int *x3, int *y3, int *x4, int *y4,int win)
{//w参数表示当前是针对哪个窗口
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
void draw_hor_line(int y,int x1,int x2,unsigned int c,int w)//画水平线
{
    if(!in_screen(x1,y,w)||!in_screen(x2,y,w))//不在屏幕内什么也不画
        return;
    //确定绘制窗口
    unsigned int *buffer=w?draw_fb:screen_fb;
    unsigned int *p1 = buffer+y*WINDOW_W+x1;
    unsigned int *p2 = buffer+y*WINDOW_W+x2;
    while (p1<=p2){
        *p1 = c;
        p1++;
    }
    draw_pixel(y,x1,RGB(34,212,153),w);
}
void draw_ver_line(int x,int y1,int y2,unsigned int c,int w)//画垂直线
{
    //确定绘制窗口
    int win_h=w?draw_h:screen_h;
    int win_w=w?draw_w:screen_w;
    unsigned int *buffer=w?draw_fb:screen_fb;
    if(x>=win_w||x<0) return;
    //超出屏幕也能画部分点
    if(y1>y2){//交换y1 y2
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
void draw_line(int x1, int y1, int x2, int y2, unsigned int color,int w)//Bresenham法画线
{
    if(!in_screen(x1,y1,w)||!in_screen(x2,y2,w))
        clip_line(x1,y1,x2,y2,&x1,&y1,&x2,&y2,w);
    int deltax = ABS(x2 - x1); //坐标之间的差值
    int deltay = ABS(y2 - y1);
    int x = x1;//线起点
    int y = y1;
    int xinc1, xinc2, yinc1, yinc2, den, num, numadd, numpixels, curpixel;
    if(x2 >= x1){//x值在不断增长
        xinc1 = 1;
        xinc2 = 1;
    }else{//x在持续减少
        xinc1 = -1;
        xinc2 = -1;
    }
    if(y2 >= y1){//y值在不断增长
        yinc1 = 1;
        yinc2 = 1;
    }else{//y在持续减少
        yinc1 = -1;
        yinc2 = -1;
    }
    if(deltax >= deltay){//对于任意y坐标均能找到对应x坐标
        xinc1 = 0;//当分子>=分母不改变x
        yinc2 = 0;//每次迭代均不改变y
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;//x多于y
    }else{//对于任意x坐标均能找到对应y坐标
        xinc2 = 0;//每次迭代均不改变x
        yinc1 = 0;//当分子>=分母不改变y
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;//y多于x
    }
    for(curpixel = 0; curpixel <= numpixels; curpixel++){
        draw_pixel(y, x, color, w);//可以画点了
        num += numadd;//增加小数的分子部分
        if (num >= den){//分子>分母
            num -= den;//计算新分母
            x += xinc1;//更新x
            y += yinc1;//更新y
        }
        x += xinc2;//更新x
        y += yinc2;//更新y
    }
}
#if 0
void draw_curve(int *points,int num, unsigned int color,int w){//画贝塞尔曲线 //待实现
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
void draw_rect(int left,int top,int right,int bottom,unsigned int c,int w){//画实心矩形
    int i,j;
    left=left<0?0:left;
    top=top<0?0:top;
    right=right<screen_w?right:screen_w-1;
    bottom=bottom<screen_h?bottom:screen_h-1;
    for(i=top;i<=bottom;i++)
        for(j=left;j<=right;j++)
            draw_pixel(i,j,c,w);
}
void draw_circle(int xc,int yc,int radius,unsigned int color,int w){//bresenham法画圆
    int t=radius>=0?1:0;
    radius=ABS(radius);
    int x = 0;//半径正数实心圆,负数空心圆
    int y = radius;
    int p = 3-(radius<<1);
    int a,b,c,d,e,f,g,h;
    int pb = yc + radius + 1, pd = yc + radius + 1;//前一个值,防止重复绘制
    if(!t)//空心圆
        while(x<=y){
            a = xc + x;//由于对称性先得到8个点
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
            if(x > 0){//防止在同一个地方画点
                draw_pixel(d, a, color, w);
                draw_pixel(b, c, color, w);
                draw_pixel(h, e, color, w);
                draw_pixel(h, g, color, w);
            }
            if(p < 0) p += (x++ << 2) + 6;
            else p += ((x++ - y--) << 2) + 10;
        }
    else//实心圆
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
int draw_text(int x,int y,int h,unsigned int c,char *s,int w){//可以改变字体大小,返回绘制的字符数
    int i=0;//注意RGB的r与b要互换位置才是CRGB,而TextOut用的是RGB
    HDC Hdc=w?draw_dc:screen_dc;//保存对应窗口临时内存上下文
    HFONT hFont;    //字体句柄
    h=(h==0)?18:h;  //字体默认大小18吧
    hFont = CreateFont(
        h,0,    //高度h, 宽取0表示由系统选择最佳值
        0, 0,    //文本倾斜与字体倾斜都为0
        FW_BOLD,    //粗体
        0,0,0,        //非斜体无下划线无中划线
        ANSI_CHARSET,    //字符集
        OUT_DEFAULT_PRECIS, //指定输出精度
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,        //一系列的默认值
        DEFAULT_PITCH | FF_DONTCARE,
        "Consolas"    //字体名称
    );
    //Hdc是绘图缓冲 hFont是字体句柄 hDC是窗口句柄 screen_handle是主窗口？
    SetBkMode(Hdc,TRANSPARENT);
    SetTextColor(Hdc,c);
    SelectObject(Hdc,hFont);//字体设置到当前设备里
    while(1){
        while(s[i]!=0&&s[i]!='\n')
            i++;//不绘制换行符
        TextOut(Hdc, x, y, s, i);
        if(s[i]=='\n'){
            if(w)//文本换行了
                plotter_y+=h;
            s=s+i+1;
            i=0;
            y+=h;//换行接着写
        }else
            break;
    }
    DeleteObject(hFont);//删除创建的字体
    return i;
}
/****************************函数图像****************************/
double origin_x=640/2;//坐标原点位于窗口坐标系的窗口坐标
double origin_y=480/2;
double coord_ratio=0.07;//缩放比例
double coord_value=0.0;//鼠标位置的值
int graph_slow=0;//是否开启了延迟渲染模式
//窗口坐标转直角坐标
void win_coor(double,double,double*,double*);
inline void win_coor(double wx,double wy,double *cx,double *cy){
    *cx=(wx-origin_x)*coord_ratio;
    *cy=(origin_y-wy)*coord_ratio;
}
//直角坐标转窗口坐标
void coor_win(double,double,int*,int*);
inline void coor_win(double cx,double cy,int *wx,int *wy){
    *wx=(int)(cx/coord_ratio+origin_x);
    *wy=(int)(origin_y-cy/coord_ratio);
}
/****************************词法分析****************************/
#define SYMBOL_SIZE 1024*8  //符号表大小
#define DATA_SIZE 1024      //程序用到的字符串与数组大小
#define NAME_SIZE 32        //标识符长度
#define PARAMETER_SIZE 8    //函数参数个数
#define CURSE_DEEP 42       //最大递归深度
#define RETURNFLAG DBL_MAX  //函数遇到返回值了
typedef struct symbol{
    int type;               //标识符赋值的数据类型
    char name[NAME_SIZE];   //标识符名称
    double value;           //整形,浮点,数组长度
    char* funcp;            //对于函数标记函数源代码位置 对于字符串表示起始地址
    double* list;           //指向数组
    struct symbol *car;     //指向左子树的标识符
    struct symbol *cdr;     //指向右子树的标识符
    int depth;              //作用域层
}symbol;//符号
typedef struct context{
    int token;
    char *locat;
}context;//代码执行位置的上下文
enum{                                                               //枚举属性符类型
    End=128, Num, Str, Array, Func, Car, Cdr, Val,                  //不带参数
    Else, If, Return, While, Clock, Mousx, Mousy, Winw, Winh, Rand, //数值变量
    Pi, E,                                                          //内置常量
    Text, Read, Sqrt, Cos, Sin, Abs, Tan ,Log, Floor,               //数值函数
    Color, Pixel, Rgb, Move, Line, Rect, Circle, Font, Flush,       //绘图函数
    Graph, Show, Slow,                                              //函数图像
    Assign, OR, AND, Equal, Sym, FuncSym, ArraySym, Void,           //二元运算符
    Nequal, LessEqual, GreatEqual, Inc, Dec                         //一元运算符
};//从Array到Circle是内置关键字,需提前加入符号表
int token_type;                 //当前属性符类型
double token_value;             //属性符值 整形、浮点
char *token_str;                //属性符地址 指向字符串
symbol *token_sym;              //属性符地址 指向标识符
symbol symbol_list[SYMBOL_SIZE];//标识符表
int symbol_num = 0;             //标识符表数量,先存再加
char data[DATA_SIZE]="";        //字符串与数组集
int data_num = 0;               //当前字符串长度
int curse_depth = 0;            //递归深度
char *pode = 0;                 //当前正在处理的代码
char *rode = 0;                 //已经确定是正确的代码,用于显示代码错误的位置
double return_value = 0.0;      //函数的返回值
double cycle = 0.0;             //运行周期数
void find_symbol(char *name){//在标识符内找标识符名称,找不到就加入
    for(int i=symbol_num-1;i>=0;i--){//在标识符表里面找这个名字是否出现过 倒着找是为了优先匹配函数递归调用同名标识符的最近声明者
        if(strcmp(name,symbol_list[i].name)==0){//找到了
            if(symbol_list[i].type==Num){//将标识符替换为找到的值
                if(symbol_list[i].depth!=0&&symbol_list[i].depth!=curse_depth)//非全局变量且变量作用域不一样,视为新变量
                    break;
                token_sym=symbol_list+i;
                token_type=Sym;//数值标识符
            }
            else if(symbol_list[i].type==FuncSym){//标识符是一个函数名称
                token_sym=symbol_list+i;
                token_type=FuncSym;//函数标识符
            }
            else if(symbol_list[i].type==ArraySym){
                token_sym=symbol_list+i;
                token_type=ArraySym;//数组标识符
            }
//            else if(symbol_list[i].type==ConsSym){
//                token_sym=symbol_list+i;
//                token_type=ConsSym;//列表标识符
//            }
            else if(symbol_list[i].type==Str){
                token_str=symbol_list[i].funcp;
                token_type=Str;//字符串标识符
            }else{
                if(symbol_list[i].type==Void){
                    token_type=Sym;
                    token_sym=symbol_list+i;
                }
                else token_type=symbol_list[i].type;//系统自带函数
            }
            return;
        }
    }
    strcpy(symbol_list[symbol_num].name,name);//标识符之前没有,于是加入标识符表
    symbol_list[symbol_num].depth=curse_depth;//递归深度确保函数调用的变量只对局部可见
    symbol_list[symbol_num].type=Void;
    token_sym=symbol_list+symbol_num;
    symbol_num++;
    token_type=Sym;
}
void match(int);
void next_token(){//获取下一个标记
    while(*pode){
        if(*pode == '#'){//注释
            while (*pode != 0 && *pode != '\n')
                pode++;
        }
        else if(*pode==' '||*pode=='\n'||*pode=='\t')
            pode++;
        else if(*pode=='*'||*pode=='/'||*pode==';'||*pode=='%'||*pode=='^'||*pode==','||*pode=='('||*pode==')'||*pode=='{'||*pode=='}'||*pode=='['||*pode==']'||*pode=='.'){
            token_type=*pode++;//将自身作为属性符
            return;
        }
        else if(*pode>='0'&&*pode<='9'){//整数或浮点 均表示为浮点
            token_value=0.0f;
            while (*pode>='0'&&*pode<='9'){
                token_value = token_value * 10.0f + *pode++ - '0';
            }
            if(*pode=='.'){//浮点
                double digits=10.0f;
                pode++;
                while(*pode>='0'&&*pode<='9'){//处理小数点
                    token_value=token_value+((double)(*pode++)-'0')/digits;
                    digits*=10.0f;
                }
            }
            token_type=Num;
            return;
        }
        else if(*pode=='\''){//字符串
            token_str=data+data_num;//字符串将存储的起始地址
            pode++;//字符串长度
            while (*pode!=0&&*pode!='\''){//保存字符串
                data[data_num]=*pode;
                if(*pode=='\\'){//转义字符
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
        else if((*pode>='a'&&*pode<='z')||(*pode>='A'&&*pode<='Z')||(*pode=='_')){//标识符
            char name[NAME_SIZE];//临时存储标识符
            int name_num=0;
            while((*pode>='a'&&*pode<='z')||(*pode>='A'&&*pode<='Z')||(*pode>='0'&&*pode<='9')||(*pode=='_')){
                name[name_num]=*pode;
                pode++;
                name_num++;
            }
            name[name_num]=0;//标识符名称
            find_symbol(name);//找找标识符顺便更新token
            return;
        }
        else if(*pode=='='){//相等 赋值
            pode++;
            token_type='=';//赋值
            if(*pode=='='){
                pode++;
                token_type=Equal;//相等
            }
            return;
        }
        else if(*pode=='+'){//加 自增
            pode++;
            token_type='+';
            if(*pode=='+'){
                pode++;
                token_type=Inc;
            }
            return;
        }
        else if(*pode=='-'){//减 自减
            token_type='-';
            pode++;
            if(*pode=='-'){
                pode++;
                token_type=Dec;
            }
            return;
        }
        else if(*pode=='!'){//不等于
            pode++;
            if(*pode=='='){
                pode++;
                token_type=Nequal;
            }
            return;
        }
        else if(*pode=='<'){//小于 小于等于
            token_type='<';
            pode++;
            if (*pode=='='){
                pode++;
                token_type=LessEqual;
            }
            return;
        }
        else if(*pode=='>'){//大于 大于等于
            pode++;
            token_type='>';
            if(*pode=='='){
                pode++;
                token_type=GreatEqual;
            }
            return;
        }
        else if(*pode=='|'){//或运算 逻辑或
            pode++;
            token_type='|';
            if (*pode=='|'){
                pode++;
                token_type=OR;
            }
            return;
        }
        else if(*pode=='&'){//与运算 逻辑与
            pode++;
            token_type='&';
            if (*pode=='&'){
                pode++;
                token_type=AND;
            }
            return;
        }else
            match(End);//肯定是搞错了
    }
    token_type=End;
}
//给定指向源代码的位置指针返回代码对应的行列
void get_posion(char *ptr,int *row,int *col){
    /**由于可怕的多线程,不能直接使用多线程同时使用的全局变量,只能一次性赋值**/
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
//检查属性符是否匹配顺便检测语法错误
void match(int token){//可以理解为处理掉这个符号,于是得到下一个符号
    if(token_type==token){
        rode=pode;
        next_token();//返回下一个属性符
    }
    else{
        wrong=token;
        wrong_type=token_type;
        get_posion(rode,&wrong_row,&wrong_col);//获取错误地点
        longjmp(jump,1);//跳转到主函数,不再执行后续代码
    }
}
/****************************语法分析****************************/
double expression();
double factor();
double term();
double function();
int boolOR();
//递归计算表达式
double term(){//计算乘除次幂
    double temp=factor();
    while(token_type=='*'||token_type=='/'||token_type=='^'){
        if(token_type=='*'){
            match('*');
            temp*=factor();
        }
        else if(token_type=='/'){
            match('/');
            double divid=factor();
            temp=fequal(divid,0.0)?0.0:temp/divid;//定义除数为0,结果为0
        }else{
            match('^');
            temp=pow(temp,factor());
        }
    }
    return temp;
}
double factor(){//标识符或内置函数求值
    double temp=0;
    if(token_type=='('){
        match('(');
        temp=expression();//计算表达式整体的值
        match(')');
    }
    else if(token_type==Num){
        temp=token_value;
        match(Num);
    }
    else if(token_type==Sym){//标识符
        temp=token_sym->value;
        match(Sym);
    }
    else if(token_type==FuncSym){
        return function();//函数求值
    }
    else if(token_type==ArraySym){
        symbol* ptr=token_sym;
        match(ArraySym);
        match('[');//期待属性符[ 若匹配取下一个属性符
        int index=(int)expression();//返回[]括号内的数值
        if(index>=0&&index<ptr->value)//下标未越界
            temp=ptr->list[index];
        match(']');
    }
//    else if(token_type==ConsSym){//列表求值
//        symbol* ptr=token_sym;
//        match(ConsSym);
//        if(token_type=='.'){
//            match('.');
//            if(token_type==Car){//左子树
//                match(Car);
//                if(ptr->funcp==NULL)//试图引用未初始化的对象
//                    temp=0.0;
//                else
//                    temp=((symbol*)ptr->funcp)->value;
//            }else if(token_type==Cdr){//右子树
//                match(Cdr);
//                if(ptr->list==NULL)//试图引用未初始化的对象
//                    temp=0.0;
//                else
//                    temp=((symbol*)ptr->list)->value;
//            }
//        }else{//列表本身的返回值是自身在标识符表的地址
//            temp=ptr->value;
//        }
//    }
    else if(token_type==Sin){//计算内置函数的sin
        double tmp;
        match(Sin);
        match('(');
        tmp=expression();
        match(')');
        return sin(tmp);//返回计算结果
    }else if(token_type==Cos){//计算内置函数的cos
        double tmp;
        match(Cos);
        match('(');
        tmp=expression();
        match(')');
        return cos(tmp);//返回计算结果
    }else if(token_type==Tan){//计算内置函数的tan
        double tmp;
        match(Tan);
        match('(');
        tmp=expression();
        match(')');
        return tan(tmp);//返回计算结果
    }else if(token_type==Log){//计算内置函数的log
        double x,y;
        match(Log);
        match('(');
        x=expression();
        match(',');
        y=expression();
        match(')');
        return log(y)/log(x);//返回x为底y的对数
    }else if(token_type==Floor){//地板函数
        double x;
        match(Floor);
        match('(');
        x=expression();
        match(')');
        return (long long)x;//返回x为底y的对数
    }else if(token_type==Rgb){//rgb颜色必须保存在24bit的空间
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
    }else if(token_type==Sqrt){//计算内置函数的sqrt
        double tmp;
        match(Sqrt);
        match('(');
        tmp=expression();
        match(')');
        return sqrt(tmp);//返回计算结果
    }else if(token_type==Abs){//计算内置函数的abs
        double tmp;
        match(Abs);
        match('(');
        tmp=expression();
        match(')');
        return ABS(tmp);//返回计算结果
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
double expression(){//返回([表达式])的值
    double temp=term();//按运算优先级逐渐增大排序
    while(token_type=='+'||token_type=='-'||token_type=='%'){//只针对加减,若得到其它属性符如])则返回
        if(token_type=='+'){
            match('+');//获取下一个属性符
            temp+=term();//加上优先级更高的 乘除运算值 或者是 某个标识符的值
        }
        else if(token_type=='-'){
            match('-');
            temp-=term();
        }else{
            match('%');
            int cdr=(int)term();
            if(cdr==0)//可不能模0啊
                temp=0;
            else
                temp=(int)temp%cdr;
        }
    }
    return temp;
}
int boolexp(){//计算布尔值
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
void skipbracket(){//跳过小括号
    if(token_type=='(')
        token_type=*pode++;
    int count=0;
    while(token_type&&!(token_type==')'&&count==0)){
        if(token_type==')')count++;//开始括号嵌套
        if(token_type=='(')count--;
        token_type=*pode++;
    }
    match(')');
}
int boolAND(){
    int val=boolexp();//AND优先级在其余之后
    while(token_type==AND){
        match(AND);
        if(val==0)//第一个表达式为0直接返回0
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
            return 1;//第一个表达式为1直接返回1
        }
        val=val|boolAND();
    }
    return val;
}
void skipbrace(){//跳过大括号
    if(token_type=='{')
        token_type=*pode++;
    int count=0;
    while(token_type&&!(token_type=='}'&&count==0)){
        if(token_type=='}')count++;//开始括号嵌套
        if(token_type=='{')count--;
        token_type=*pode++;
    }
    match('}');//注意这里处理掉了},于是调用skipbrace的部分不要处理掉}
}
double statement(){//执行一条代码
    if(token_type=='{'){//既然是大括号,要么是函数要么是if while
        match('{');
        while(token_type!='}'){
            if(fequal(RETURNFLAG,statement()))//递归执行大括号内部代码
                return RETURNFLAG;
        }
        match('}');//若函数本身没有返回值
    }
    else if(token_type==If){
        match(If);
        match('(');//期待括号并将括号内部解释为布尔变量
        int boolresult=boolOR();
        while(token_type!='{'&&token_type!=End)//跳过后续括号
            next_token();
        //match(')');
        if(boolresult){//if为真继续执行后面大括号的代码,后面遇到else就要跳过大括号
            if(fequal(RETURNFLAG,statement()))//执行代码
                return RETURNFLAG;//若有返回值说明是函数
        }
        else skipbrace();//if为假不执行大括号
        if(token_type==Else){
            match(Else);
            if(!boolresult){
                if(fequal(RETURNFLAG,statement()))
                    return RETURNFLAG;
            }
            else skipbrace();//if后面的大括号只能执行一个
        }
    }
    else if(token_type==While){
        match(While);
        char* while_start=pode;//while函数后面的()作为潜在的循环起始位置
        int boolresult;
        do {
            pode=while_start;//回到开始重新判断while条件
            token_type='(';//用于执行完大括号再回到开始
            match('(');//判断while是否为1
            boolresult=boolOR();
            match(')');//现在token_type为{
            if(boolresult){
                if(fequal(RETURNFLAG,statement()))//执行大括号代码
                    return RETURNFLAG;//若遇到返回值直接返回
            }
            else skipbrace();//不执行大括号
        }while(boolresult);
    }
    else if(token_type==Sym||token_type==ArraySym){//字符串 整型 浮点 数组
        symbol* s=token_sym;//abc = 123; or abc++;
        int token=token_type;
        int index;
        match(token);
        if(token==ArraySym){//数组未在表达式出现说明是赋值
            if(token_type=='='){//一连串赋值
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
        }else if(token_type=='.'){//改变左右子树 或实际值
            match('.');
            if(token_type==Car){
                match(Car);
                match('=');
                s->car=(symbol*)(int)expression();//赋值为地址
            }
            else if(token_type==Cdr){
                match(Cdr);
                match('=');
                s->cdr=(symbol*)(int)expression();//赋值为地址
            }
            else{
                match(Val);
                match('=');
                s->value=expression();//赋值为实值
            }
        }else{
            if(token_type=='='){//数值 字符串 列表 赋值
                match('=');
                if(token_type==Str){//字符串初始化
                    s->funcp=token_str;//将字符串标识符映射为指向字符串起始地址的指针
                    s->type=Str;
                    match(Str);
//                }else if(token_type==ConsSym){//引用列表
//                    s->type=ConsSym;
//                    s->value=(int)token_sym;
                }else{//否则是数值标识符赋值
                    s->value=expression();//计算数字后面的表达式
                    s->type=Num;
                }
            }else{//则是数值自增或自减
                if(token_type==Inc){
                    match(Inc);
                    s->value+=1.0f;//计算数字后面的表达式
                }else{
                    match(Dec);
                    s->value-=1.0f;
                }
            }
        }
        match(';');
    }
//    else if(token_type==ConsSym){//列表的实质是指向标识符的指针
//        symbol* s=token_sym;
//        match(ConsSym);
//        if(token_type=='.'){
//            match('.');
//            if(token_type==Car){//左子树赋值
//                match(Car);
//                match('=');
//                s->funcp=(char*)token_sym;//引用一个标识符
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
//            //后面必须得是列表
//            symbol* t=token_sym;
//            match(ConsSym);
//            s->funcp=t->funcp;
//            s->list=t->list;
//        }
//        match(';');
//    }
    else if(token_type==Func){//函数标识符 func fun{}
        match(Func);
        match(Sym);
        symbol* s=token_sym;//其实是函数定义
        s->type=FuncSym;
        s->funcp=pode;//将函数定义的起始位置保存起来,pode是第一个参数的位置
        match('(');
        skipbracket();
        s->value=token_type;//这个的值是{
        skipbrace();//未调用函数于是先返回
    }
//    else if(token_type==Cons){//生成列表
//        match(Cons);
//        match(Sym);
//        symbol* s=token_sym;
//        s->type=ConsSym;//定义列表对象
//        s->list=NULL;
//        s->funcp=NULL;
//        s->value=(int)s;//保存的是该列表的标识符地址方便后面引用
//        match(';');
//    }
    else if(token_type==Array){//声明数组array mat(10)
        match(Array);
        symbol* s=token_sym;
        match(Sym);
        match('(');
        int length=(int)expression();//数组大小
        match(')');
        s->list=(double*)data+data_num;//指针多用
        s->value=length;
        s->type=ArraySym;
        data_num+=length*sizeof(double);
        match(';');
    }
    else if(token_type==Return){//函数返回 return(5)
        match(Return);
        match('(');
        return_value=expression();//计算括号内的值
        match(')');
        match(';');
        return RETURNFLAG;
    }else if(token_type==Winw){//更改窗口大小
        int tmp=0;
        match(Winw);
        match('=');
        tmp=expression();//计算新的窗口值
        match(';');
        if(tmp==draw_w)//仅当窗口尺寸改变才修改窗口大小
            return 0;
        if(tmp>=200&&tmp<=1920){
            draw_w=tmp;
            RECT exclude = {}, include = {};//漂移修正
            GetClientRect(draw_handle,&exclude);//获取窗口逻辑尺寸,右下逻辑坐标
            GetWindowRect(draw_handle,&include);//获取窗口实际左上与右下坐标,包括非客户区和阴影
            int width = include.right-include.left-exclude.right;//漂移量
            int height = include.bottom-include.top-exclude.bottom;
            SetWindowPos(draw_handle, NULL, include.left, include.top, width+draw_w, height+draw_h, SWP_SHOWWINDOW);//左上与实际尺寸
        }
    }else if(token_type==Winh){//更改窗口大小
        int tmp=0;
        match(Winh);
        match('=');
        tmp=expression();//计算新的窗口值
        match(';');
        if(tmp==draw_h)//仅当窗口尺寸改变才修改窗口大小
            return 0;
        if(tmp>=100&&tmp<=1080){
            draw_h=tmp;
            RECT exclude = {}, include = {};//漂移修正
            GetClientRect(draw_handle,&exclude);//获取窗口逻辑尺寸,右下逻辑坐标
            GetWindowRect(draw_handle,&include);//获取窗口实际左上与右下坐标,包括非客户区和阴影
            int width = include.right-include.left-exclude.right;//漂移量
            int height = include.bottom-include.top-exclude.bottom;
            SetWindowPos(draw_handle, NULL, include.left, include.top, width+draw_w, height+draw_h, SWP_SHOWWINDOW);//左上与实际尺寸
        }
    }else if(token_type==Text){//自带函数
        int old_num=data_num;//text里面的临时字符串不能永久保存在列表里面
        char str[DATA_SIZE/2]="";
        double temp;
        match(Text);
        match('(');
        while(token_type!=';'){//只要没超出text函数参数
            if(token_type==Str){//可以输出 (函数值,字符串,标识符值,数值)
                sprintf(str,"%s%s",str,token_str);
                match(Str);
            }else{
                temp=expression();
                if(fequal(temp,(long long)temp))//是整数
                    sprintf(str,"%s%I64d",str,(long long)temp);
                else
                    sprintf(str,"%s%.2lf",str,temp);
            }
            if(token_type==',')//后面还有要打印的
                match(',');
            else//否则直接报错
                match(')');
        }
        draw_text(plotter_x,plotter_y,plotter_font,((plotter_color>>16)|((plotter_color&0x000000ff)<<16)|(plotter_color&0x0000ff00)),str,1);
        match(';');
        data_num=old_num;
    }else if(token_type==Pixel){//自带函数
        int x,y,c;
        match(Pixel);
        match('(');
        y=(int)expression();//横坐标
        match(',');
        x=(int)expression();//纵坐标
        match(',');
        c=(int)expression();//颜色
        draw_pixel(x,y,c,1);
        match(')');
        match(';');
    }else if(token_type==Color){//自带函数
        match(Color);//更改显示颜色
        match('(');
        plotter_color=(int)expression();
        match(')');
        match(';');
    }else if(token_type==Move){//自带函数
        match(Move);//线条起始点
        match('(');
        plotter_x=(int)expression();
        match(',');
        plotter_y=(int)expression();
        match(')');
        match(';');

    }else if(token_type==Line){//自带函数
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
    }else if(token_type==Rect){//自带函数
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
        draw_rect(x1,y1,x2,y2,plotter_color,1);//画矩形
        match(')');
        match(';');
    }else if(token_type==Circle){//自带函数
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
    }else if(token_type==Font){//自带函数
        int h;
        match(Font);//改变字体高度
        match('(');
        h=(int)expression();
        plotter_font=h;
        match(')');
        match(';');
    }else if(token_type==Flush){//自带函数
        match(Flush);//强制刷新绘图
        match('(');
        match(')');
        match(';');
        HDC hDC = GetDC(draw_handle);//刷新绘图窗口
        BitBlt(hDC, 0, 0, draw_w, draw_h, draw_dc, 0, 0, SRCCOPY);
        ReleaseDC(draw_handle, hDC);
        memset(draw_fb, 0, WINDOW_W * WINDOW_H * 4);
        draw_fps=get_fps();//画一次画就更新一次fps
        wrong=0;//假设代码没出错
    }else if(token_type==Graph){//绘制函数图像
        int dimension=0;//是要画几维图像呢
        context start;//graph定义起始位置
        int input_x=0;//输入参数在符号表的位置
        int input_y=0;
        curse_depth++;
        match(Graph);
        match('(');
        if(token_type!=')'){
            if(token_type==Sym)//获取参数x
                input_x=symbol_num-1;
            match(Sym);
            dimension++;
            if(token_type==','){
                match(',');
                if(token_type==Sym)//获取参数y
                    input_y=symbol_num-1;
                match(Sym);
                dimension++;
            }
        }
        match(')');
        start=(context){token_type,pode};//指向{开始的位置
        match('{');
        if(dimension==1){//画显函数图像
            int y[WINDOW_W];//临时保存结果
            for(int i=0;i<draw_w;i++){
                double cx,cy;//实际坐标
                int wx,wy;//窗口坐标
                win_coor(i,0.0,&cx,&cy);
                symbol_list[input_x].value=cx;
                token_type=start.token;
                pode=start.locat;
                statement();
                if(i==mouse_x)
                    coord_value=return_value;//鼠标位置的值
                coor_win(cx,return_value,&wx,&wy);
                y[i]=wy;
            }
            for(int i=1;i<draw_w;i++){//将图像画出来
                draw_line(i-1,y[i-1],i,y[i],plotter_color,1);
            }
        }else if(dimension==2){//画隐函数图像
            int color;
            if(graph_slow){//我将其称为延迟渲染
                static unsigned int graph_buffer[WINDOW_H][WINDOW_W]={0};
                static int i=0;
                for(int j=0;j<draw_h;j++){
                    double cx,cy;//实际坐标
                    win_coor(i,j,&cx,&cy);
                    symbol_list[input_x].value=cx;
                    symbol_list[input_y].value=cy;
                    token_type=start.token;
                    pode=start.locat;
                    statement();
                    if(i==mouse_x&&j==mouse_y)
                        coord_value=return_value;//鼠标位置的值
                    int R=plotter_color>>16;//正色
                    int G=(plotter_color&0x0000ff00)>>8;
                    int B=plotter_color&0x000000ff;
                    if(return_value>=1.0)
                        color=plotter_color;//正色
                    else if(return_value<=0.0)
                        color=CRGB(255-R,255-G,255-B);//反色
                    else
                        color=CRGB(255-R+(int)((2*R-255)*return_value),
                                   255-G+(int)((2*G-255)*return_value),
                                   255-B+(int)((2*B-255)*return_value));
                    graph_buffer[j][i]=color;
                }
                i=draw_w-i-1;
                if(i<0||i>=draw_w) i=draw_w-1;//i的值可能因为前面执行的中断出现异常
                for(int j=0;j<draw_h;j++){
                    double cx,cy;//实际坐标
                    win_coor(i,j,&cx,&cy);
                    symbol_list[input_x].value=cx;
                    symbol_list[input_y].value=cy;
                    token_type=start.token;
                    pode=start.locat;
                    statement();
                    if(i==mouse_x&&j==mouse_y)
                        coord_value=return_value;//鼠标位置的值
                    int R=plotter_color>>16;//正色
                    int G=(plotter_color&0x0000ff00)>>8;
                    int B=plotter_color&0x000000ff;
                    if(return_value>=1.0)
                        color=plotter_color;//正色
                    else if(return_value<=0.0)
                        color=CRGB(255-R,255-G,255-B);//反色
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
                memcpy(draw_fb,graph_buffer,WINDOW_H*WINDOW_W*sizeof(unsigned int));//复制到屏幕缓冲
            }else{
                for(int i=0;i<draw_w;i++)
                    for(int j=0;j<draw_h;j++){
                        double cx,cy;//实际坐标
                        win_coor(i,j,&cx,&cy);
                        symbol_list[input_x].value=cx;
                        symbol_list[input_y].value=cy;
                        token_type=start.token;
                        pode=start.locat;
                        statement();
                        if(i==mouse_x&&j==mouse_y)
                            coord_value=return_value;//鼠标位置的值
                        int R=plotter_color>>16;//正色
                        int G=(plotter_color&0x0000ff00)>>8;
                        int B=plotter_color&0x000000ff;
                        if(return_value>=1.0)
                            color=plotter_color;//正色
                        else if(return_value<=0.0)
                            color=CRGB(255-R,255-G,255-B);//反色
                        else
                            color=CRGB(255-R+(int)((2*R-255)*return_value),
                                       255-G+(int)((2*G-255)*return_value),
                                       255-B+(int)((2*B-255)*return_value));
                        draw_pixel(j,i,color,1);
                    }
            }
        }
        #if 1
        else if(dimension==0){//用于测试
            static double x1=0;
            x1=sin(cycle/1000);
            for(int i=0;i<draw_w;i++)
                for(int j=0;j<draw_h;j++){
                    double x,y;//实际坐标
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
        else if(dimension==0){//用于测试
            int r=255,g=255,b=0,col;
            for(int i=0;i<draw_w;i++)
                for(int j=0;j<draw_h;j++){
                    double x,y;//实际坐标
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
                        col=CRGB(r,g,b);//正色
                    else if(c<=0.0)
                        col=CRGB(255-r,255-g,255-b);//反色
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
        skipbrace();//跳出大括号
        while(symbol_list[symbol_num-1].depth==curse_depth)//调用函数开辟的新内存要清空
            symbol_num--;
        curse_depth--;
    }else if(token_type==Show){//绘制坐标系并在鼠标处给出坐标系坐标
        double x,y;
        int px=plotter_x,py=plotter_y;//不希望改变plotter的坐标
        char str[20]="";
        match(Show);
        match('(');
        match(')');
        match(';');
        draw_hor_line(origin_y,0,draw_w-1,plotter_color,1);//坐标轴
        draw_ver_line(origin_x,0,draw_h-1,plotter_color,1);
        win_coor(mouse_x,mouse_y,&x,&y);
        sprintf(str,"x:%.4lf\ny:%.4lf\nv:%.4lf",x,y,coord_value);
        draw_text(mouse_x,mouse_y,plotter_font,((plotter_color>>16)|((plotter_color&0x000000ff)<<16)|(plotter_color&0x0000ff00)),str,1);
        plotter_x=px;
        plotter_y=py;
    }else if(token_type==Slow){//改变绘图模式
        match(Slow);
        match('(');
        graph_slow=(int)expression();
        match(')');
        match(';');
    }else{
        factor();//调用了函数 并且即使函数有返回值也没有回代赋值 因此接下来一定得匹配语句结束
        match(';');//不论如何只要不匹配一定表示代码有问题
    }
    return 0;
}
double function(){//执行一个函数并返回函数值 有意思的是，由于c语言本身是递归调用的，因此自定义函数本身也是递归调用的
    if(curse_depth>CURSE_DEEP)
        match(End);//超出最大递归深度
    return_value=0.0f;
    symbol* s=token_sym;//函数本身的标识符
    match(FuncSym);
    match('(');
    //先将函数参数值全部求出
    double parameter[PARAMETER_SIZE]={0.0};
    int para=0;//参数个数
    while(token_type!=')'&&token_type!=End){
        if(para==PARAMETER_SIZE)
            match(End);//传参过多
        parameter[para]=expression();
        para++;
        if(token_type==',')
            match(',');
    }
    match(')');
    context cont_call={token_type,pode};//保存当前调用函数的源代码位置上下文以便后面返回
    pode=s->funcp;//PC程序计数器跳转到函数定义的位置,即第1个参数的位置
    token_type='(';
    match('(');
    curse_depth++;//嵌套层数++ 针对函数变量的作用域
    int pa=0;
    while(token_type!=')'&&token_type!=End){//将函数参数中的变量按函数定义顺序插入函数作用域
        if(pa==PARAMETER_SIZE)
            match(End);//传参过多
        symbol_list[symbol_num]=*token_sym;//将函数定义时的参数标识符插入函数作用域
        strcpy(symbol_list[symbol_num].name,token_sym->name);
        symbol_list[symbol_num].depth=curse_depth;
        symbol_list[symbol_num].value=parameter[pa];//最后给定义定义标识符赋值为函数参数值,函数参数可以是标识符或任意有返回值的数值
        symbol_num++;
        pa++;//传参数过大则报错,传参数小则赋0
        match(Sym);
        if(token_type==',')
            match(',');//不断插入函数后面带的参数
    }
    if(pa<para)
        match(End);//传参过多
    match(')');
    token_type=(int)s->value;
    statement();//执行函数了哈
    pode=cont_call.locat;//回来啦
    token_type=cont_call.token;//恢复属性符
    while(symbol_list[symbol_num-1].depth==curse_depth)//调用函数开辟的新内存要清空
        symbol_num--;
    curse_depth--;
    return return_value;
}
/****************************信息缓冲****************************///暂时无用
typedef struct{
    char name[32][32];//最多支持32个缓冲吧
    unsigned int data[32];//附加信息
    unsigned char top;//实时更新的栈，先进先销毁的队列
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
    msg->top=(msg->top+1)%32;//top先存后加
}
int msg_get(Message *msg,int i){//获取第i条信息的实际位置，用于遍历
    return (msg->top-msg->num+32+i)%32;
}
/****************************程序信息****************************/
int get_line(){//获取该行到光标处的字符数,,
    int tmp_c=cursor-1,line=0;
    while(tmp_c>=0&&code[tmp_c]!='\n'){
        line++;
        tmp_c--;
    }
    return line;
}
void get_code(char c){//在光标处插入一个字符或删除字符
    if(c==0){//正着删
        for(int i=cursor-1;i<node;i++)
            code[i]=code[i+1];
        cursor--;
        node--;
        return;
    }
    if(c==-1){//倒着删
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
//    if (curTime - lastTime > 1000){ // 取固定时间间隔为1秒
//        fps = frameCount;//因为是0.5秒刷新一次要乘2
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
char *key_word(int n){//属性符类型与字符串转换表
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
//代码高亮关键词与颜色
//注释 定义 关键字 函数 字符 数字
unsigned int key_color[]={RGB(200,191,231),RGB(255,242,0),RGB(0,162,232),RGB(199,71,199),RGB(255,127,39),RGB(181,230,29)};
void draw_segment(char *s,int x,int y){//给我一个段将它打印出来
    //注释 定义 关键字 函数 字符 数字
    if(*s=='#')
        draw_text(x,y,18,key_color[0],s,0);//注释
    else if(*s>='0'&&*s<='9')
        draw_text(x,y,18,key_color[5],s,0);//数字
    else if(*s<'A'||(*s>'Z'&&*s<'a')||*s>'z')
        draw_text(x,y,18,key_color[4],s,0);//字符
    else{
        int i=0;
        for(i=Array;i<Assign;i++)//代码高亮的范围
            if(strcmp(s,key_word(i))==0)
                break;
        if(i==Assign)
            draw_text(x,y,18,0x00ffffff,s,0);//普通标识符
        else if(i<Else)
            draw_text(x,y,18,key_color[1],s,0);//定义
        else if(i<Text)
            draw_text(x,y,18,key_color[2],s,0);//关键字
        else
            draw_text(x,y,18,key_color[3],s,0);//函数
    }
}
//错误提示
void draw_code(){
    //画代码行数
    for(int i=line_start+1,ls=18;i<=line_start+25;i++,ls+=18){
        char s[4]="";
        sprintf(s,"%d",i);
        draw_text(0,ls,0,RGB(34,177,76),s,0);
    }
    //若代码执行出错,输出错误信息,将执行出错的位置标红
    if(wrong){
        /**由于可怕的多线程,千万不要轻易修改多线程共享的全局变量**/
        int row=(wrong_row-line_start)*18;//去掉校准行
        int col=wrong_col*8;
        char wrong_code[32]="Wrong:";//将输出错误描述信息
        if(wrong_type<128)//检测到
            sprintf(wrong_code,"%s%c ",wrong_code,(char)wrong_type);
        else
            sprintf(wrong_code,"%s%-6s ",wrong_code,key_word(wrong_type));
        if(wrong<128)//期望值
            sprintf(wrong_code,"%sRight:%c ",wrong_code,(char)wrong);
        else
            sprintf(wrong_code,"%sRight:%-6s ",wrong_code,key_word(wrong_type));
        draw_rect(col+20,row,screen_w-30,row+18,CRGB(255,0,0),0);
        draw_text(180,0,0,RGB(34,197,76),wrong_code,0);
    }
    //画光标
    if((cursor_r-line_start+2)*18>screen_h)//光标跑下面去了
        line_start++;
    else if(cursor_r<line_start)
        line_start--;
    draw_hor_line((cursor_r-line_start+2)*18,cursor_c*8+20,cursor_c*8+27,0x00ffffff,0);
    //画代码
    char*p=code;//指向当前输出的代码
    char str[DATA_SIZE/2]="";//即将送入显示的字符段
    int x=0,y=18,len,tmp=0;//应当显示的代码起始坐标
    if(line_start)//上面还有代码
        draw_text(screen_w-30,18,30,RGB(255,201,14),"↑",0);
    while(tmp<line_start)//从起始行开始显示
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
            if(y+18>screen_h){//超出屏幕显示范围了
                draw_text(screen_w-30,screen_h-30,30,RGB(255,201,14),"↓",0);
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
    //最后标出当前代码在执行哪一行
    int row,col;
    get_posion(rode,&row,&col);
    draw_text(0,(row-line_start)*18-8,30,RGB(255,201,14),"→",0);
}
/*******the*hitchhikers*guide*to*the*galaxy*******/
void car_init(int wx,int wy,char *s){
    win32_init(wx,wy,s);
    node=strlen(code);
    srand(time(NULL));
}

void car_con(){
    //键盘
    if(is_press && press_time<20){
        press_time++;
        return;//只触发一次或触发很多次
    }
    int shift=0;//大小写
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
        //数当前行的空格数以便对齐
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
            if(cursor>2&&code[cursor-1]==' '&&code[cursor-2]==' '&&code[cursor-3]==' '){//去掉tab键
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
    if(!screen_keys[VK_CONTROL])//输入代码时不能按其它键
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
        if(screen_keys[KEY_V]){//粘贴
            //先看能否打开剪贴板
            if(OpenClipboard(screen_handle)){
                //获取剪贴板数据,仅支持ANSI文本
                HANDLE hClipboardData = GetClipboardData(CF_TEXT);
                //指针指向剪贴板数据
                char *pchData = (char*)GlobalLock(hClipboardData);
                //更新至代码
                while(*pchData){
                    if(*pchData!='\r')
                        get_code(*pchData);
                    pchData++;
                }
                //解锁全局内存
                GlobalUnlock(hClipboardData);
                //关闭剪贴板以便其它程序使用
                CloseClipboard();
            }
            is_press=1;
        }
        if(screen_keys[KEY_C]){//复制全部
            //先看能否打开剪贴板
            if (OpenClipboard(screen_handle)){
                //清空剪贴板
                EmptyClipboard();
                //为剪贴板分配内存
                HGLOBAL hClipboardData;
                hClipboardData = GlobalAlloc(GMEM_DDESHARE,
                                            node+1);
                //获得指向剪贴板的指针
                char * pchData;
                pchData = (char*)GlobalLock(hClipboardData);
                //从局部变量复制到全局内存
                strcpy(pchData, code);
                //解锁内存
                GlobalUnlock(hClipboardData);
                //剪贴板数据粘贴至全局内存
                SetClipboardData(CF_TEXT,hClipboardData);
                //解锁剪贴板以便其它程序使用
                CloseClipboard();
            }
            is_press=1;
        }
        if(screen_keys[KEY_D]){//清空
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
    //鼠标
    if(mouse_lclick){
        static int mouse_ox,mouse_oy;
        int mx,my;
        if(!mouse_have_lclick){//首次按下记录鼠标坐标
            mouse_have_lclick=1;
            mouse_ox=mouse_x;
            mouse_oy=mouse_y;
            return;
        }
        mx=mouse_x-mouse_ox;//计算鼠标移动矢量
        my=mouse_y-mouse_oy;
        origin_x+=mx;
        origin_y+=my;
        mouse_ox=mouse_x;
        mouse_oy=mouse_y;
    }
    if(mouse_rclick){
        static int mouse_oy;
        int my;
        if(!mouse_have_rclick){//首次按下记录鼠标坐标
            mouse_have_rclick=1;
            mouse_oy=mouse_y;
            return;
        }
        my=mouse_y-mouse_oy;//计算鼠标移动矢量
        double delta_ratio=log(coord_ratio+1.0)*my/42.0;//coord_ratio/128.0*my; 缩放比例改变量
        double old_ratio=coord_ratio;//旧缩放比例
        coord_ratio=coord_ratio+delta_ratio;//缩放越大,缩放越慢,缩放越小,缩放越快
        origin_x=draw_w/2.0+(origin_x-draw_w/2.0)*old_ratio/coord_ratio;//坐标原点同样需要根据位置发生改变
        origin_y=draw_h/2.0+(origin_y-draw_h/2.0)*old_ratio/coord_ratio;
        mouse_oy=mouse_y;
    }
}

void *car_upd(){
    pthread_detach(pthread_self());//线程结束后自动释放资源
    // 往符号表里面添加关键词
    for(int i=Array;i<Assign;i++){
        find_symbol(key_word(i));//更新内置关键字token
        symbol_list[symbol_num-1].type=i;//更新属性符为内置标识属性符
    }
    while(car_run){
        symbol_num=Assign-Array;
        curse_depth=data_num=plotter_x=plotter_y=0;//一切重新开始
        plotter_font=18;//默认字体自号
        plotter_color=0x00ff7f27;//默认颜色是橘色
        pode=rode=code;
        //接下来在绘图区画画
        next_token();//只要有了第一个属性符
        if(setjmp(jump) == 0){//代码执行过程一旦出错跳到结束
            while (token_type!=End)
                statement();//就能持续开车往下走了
            wrong=0;/**由于多线程,不能假设代码执行过程没有出错,除非真的执行到了代码结尾**/
        }
        //仅当代码运行完毕才绘制绘图窗口,或者代码自己调用刷新窗口
        //因为绘图的时候就不能执行函数了
        HDC hDC = GetDC(draw_handle);//绘图窗口,尽量使用局部变量,多线程一定要谨慎修改共享全局变量
        BitBlt(hDC, 0, 0, draw_w, draw_h, draw_dc, 0, 0, SRCCOPY);
        ReleaseDC(draw_handle, hDC);
        memset(draw_fb, 0, WINDOW_W * WINDOW_H * 4);
        draw_fps=get_fps();//更新绘画窗的fps
        cycle=cycle+1.0;//运行周期数
    }
    pthread_exit(0);
    return NULL;
}

void car_dra(){
    //开始绘图,只画主窗口
    draw_message();
    draw_code();
    //显示绘图缓冲内容
    HDC hDC = GetDC(screen_handle);//主窗口
    BitBlt(hDC, 0, 0, screen_w, screen_h, screen_dc, 0, 0, SRCCOPY);
    ReleaseDC(screen_handle, hDC);
    win32_dispatch();//处理消息
    //最后清屏
    memset(screen_fb, 0, WINDOW_W * WINDOW_H * 4);
}

void car_close(){
    win32_close();
}

int main(void){
    car_init(screen_w,screen_h,"ISLANDS");
    //宁愿在线程里死循环也不要在循环里创线程
    pthread_t tid;//为执行代码部分单独创建线程
    pthread_create(&tid,NULL,car_upd,NULL);//代码执行
    while(car_run){
        car_con();//输入代码
        car_dra();//绘制代码
        Sleep(frame);//帧速率
    }
    car_close();
    return 0;
}
