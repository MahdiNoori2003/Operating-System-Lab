// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct
{
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do
  {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c)
    {
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if ((s = (char *)*argp++) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

void panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for (;;)
    ;
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CURSORBACK 0x101
#define SHIFTRIGHT 0x102
#define SHIFTLEFT 0x103
#define CURSORFORWARD 0x104
#define CURSORRESET 0x105
#define CLEAR 0x106

#define CRTLEN 80 * 25
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000); // CGA memory

#define END_OF_ARRAY '\0'
#define END_OF_LINE '\n'

#define INPUT_BUF 128
struct
{
  char buf[INPUT_BUF];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
  uint pointer;
} input;

#define CMD_BUF_SIZE 10
struct
{
  char buf[CMD_BUF_SIZE][INPUT_BUF];
  uint r; // buffer head index
  uint w; // Write index
  uint pointer;
  uint s;        // buffer size
  uint movement; // movement in relation to head
} cmd_buffer;

#define C(x) ((x) - '@') // Control-x
#define ARROW_UP 0xE2
#define ARROW_DOWN 0xE3

#define CAN_ARROW_UP (cmd_buffer.movement >= cmd_buffer.s) ? 0 : 1
#define CAN_ARROW_DOWN (cmd_buffer.movement <= 1) ? 0 : 1

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == END_OF_LINE)
    pos += 80 - pos % 80;
  else if (c == BACKSPACE)
  {
    if (pos > 0)
      --pos;
    crt[pos] = ' ' | 0x0700;
  }
  else if (c == CURSORBACK)
  {
    if (pos > 0)
      --pos;
  }
  else if (c == CURSORFORWARD)
  {
    pos++;
  }
  else if (c == SHIFTRIGHT)
  {
    for (int i = (pos + input.e - input.pointer); i > pos; i--)
    {
      crt[i] = crt[i - 1];
    }
  }
  else if (c == SHIFTLEFT)
  {
    for (int i = pos - 1; i < (pos + input.e - input.pointer); i++)
    {
      crt[i] = crt[i + 1];
    }
    crt[pos + input.e - input.pointer] = ' ';
    pos--;
  }
  else if (c == CURSORRESET)
  {
    pos += input.e - input.pointer;
    while (pos % 80 != 2)
    {
      crt[--pos] = ' ' | 0x0700;
    }
  }

  else if (c == CLEAR)
  {
    for (int i = 0; i <= CRTLEN; i++)
    {
      crt[i] = ' ' | 0x0700;
    }
    pos = 0;
  }

  else if (c == ARROW_UP || c == ARROW_DOWN)
  {
    for (int i = (pos % 80); i > 2; i--)
    {
      crt[pos] = ' ' | 0x0700;
      pos--;
    }
    for (int i = 0; i < (input.e - input.w) % INPUT_BUF; i++)
    {
      crt[pos] = (input.buf[(i + input.w) % INPUT_BUF] & 0xff) | 0x0700;
      pos++;
    }
  }

  else
    crt[pos++] = (c & 0xff) | 0x0700; // black on white

  if (pos < 0 || pos > 25 * 80)
    panic("pos under/overflow");

  if ((pos / 80) >= 24)
  { // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
  if (input.e == input.pointer /*|| c ==CONTROLL*/)
  {
    crt[pos] = ' ' | 0x0700;
  }
}

void consputc(int c)
{
  if (panicked)
  {
    cli();
    for (;;)
      ;
  }

  if (c == BACKSPACE)
  {
    uartputc('\b');
    uartputc(' ');
    uartputc('\b');
  }
  else
    uartputc(c);
  cgaputc(c);
}

void shiftRight()
{
  for (int i = input.e; i > input.pointer; i--)
  {
    input.buf[i % INPUT_BUF] = input.buf[(i - 1) % INPUT_BUF];
  }
}

void shiftLeft()
{
  for (int i = input.pointer - 1; i < input.e; i++)
  {
    input.buf[i % INPUT_BUF] = input.buf[(i + 1) % INPUT_BUF];
  }
}

void recordCommand()
{
  int i;
  if (cmd_buffer.s != CMD_BUF_SIZE)
  {
    for (i = 0; i < (input.e - input.w) % INPUT_BUF; i++)
    {
      if (input.buf[(i + input.w) % INPUT_BUF] != END_OF_LINE || input.buf[(i + input.w) % INPUT_BUF] != C('D'))
        cmd_buffer.buf[cmd_buffer.w][i] = input.buf[i + input.w];
    }
    cmd_buffer.buf[cmd_buffer.w][i] = END_OF_ARRAY;
    cmd_buffer.s++;
  }
  else
  {
    for (i = 0; i < (input.e - input.w) % INPUT_BUF; i++)
    {
      if (input.buf[(i + input.w) % INPUT_BUF] != END_OF_LINE || input.buf[(i + input.w) % INPUT_BUF] != C('D'))
        cmd_buffer.buf[cmd_buffer.r][i] = input.buf[i + input.w];
    }
    cmd_buffer.buf[cmd_buffer.r][i] = END_OF_ARRAY;
    cmd_buffer.r++;
    cmd_buffer.r %= 10;
  }
  cmd_buffer.w++;
  cmd_buffer.w %= 10;
  cmd_buffer.pointer = cmd_buffer.w;
  cmd_buffer.movement = 0;
}

void loadCommand()
{
  for (int i = 0;; i++)
  {
    if (cmd_buffer.buf[cmd_buffer.pointer][i] == END_OF_ARRAY)
    {
      input.pointer %= INPUT_BUF;
      input.e %= INPUT_BUF;
      break;
    }
    input.buf[(input.w + i) % INPUT_BUF] = cmd_buffer.buf[cmd_buffer.pointer][i];
    input.e++;
    input.pointer++;
  }
}

void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while ((c = getc()) >= 0)
  {
    switch (c)
    {
    case C('P'): // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'): // Kill line.
      consputc(CURSORRESET);
      input.pointer = input.e = input.w;
      break;
    case C('H'):
    case '\x7f': // Backspace
      if (input.pointer != input.w)
      {
        if (input.e != input.pointer)
        {
          shiftLeft();
          consputc(SHIFTLEFT);
        }
        else
        {
          consputc(BACKSPACE);
        }
        input.e--;
        input.pointer--;
      }
      break;
    case C('B'):
      if (input.pointer != input.w)
      {
        input.pointer--;
        consputc(CURSORBACK);
      }
      break;
    case C('L'):
      consputc(CLEAR);
      input.e = input.w;
      input.pointer = input.w;
      consputc('$');
      consputc(' ');
      break;
    case C('F'):
      if (input.pointer != input.e)
      {
        input.pointer++;
        consputc(CURSORFORWARD);
      }
      break;

    case ARROW_UP:
      if (CAN_ARROW_UP && cmd_buffer.s > 0)
      {
        input.e = input.w;
        input.pointer = input.w;
        if (cmd_buffer.pointer == 0)
        {
          cmd_buffer.pointer = 9;
        }
        else
        {
          cmd_buffer.pointer--;
        }
        cmd_buffer.movement++;
        cmd_buffer.pointer %= CMD_BUF_SIZE;
        loadCommand();
        consputc(ARROW_UP);
      }
      break;

    case ARROW_DOWN:
      if (cmd_buffer.movement != 0)
      {
        input.e = input.w;
        input.pointer = input.w;
        if (CAN_ARROW_DOWN && cmd_buffer.s > 0)
        {
          cmd_buffer.pointer++;
          cmd_buffer.movement--;
          cmd_buffer.pointer %= CMD_BUF_SIZE;
          loadCommand();
          consputc(ARROW_DOWN);
        }
        else
        {
          if (cmd_buffer.movement - 1 >= 0)
          {
            cmd_buffer.movement--;
            cmd_buffer.pointer = 1;
          }
          consputc(CURSORRESET);
        }
      }
      break;
    default:
      if (c != 0 && input.e - input.r < INPUT_BUF)
      {
        c = (c == '\r') ? END_OF_LINE : c;
        if (c == END_OF_LINE || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          input.pointer = input.e;
          recordCommand();
        }
        if (input.e != input.pointer)
        {
          shiftRight();
          consputc(SHIFTRIGHT);
        }
        input.buf[input.pointer++ % INPUT_BUF] = c;
        input.e++;
        consputc(c);
        if (c == END_OF_LINE || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if (doprocdump)
  {
    procdump(); // now call procdump() wo. cons.lock held
  }
}

int consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0)
  {
    while (input.r == input.w)
    {
      if (myproc()->killed)
      {
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D'))
    { // EOF
      if (n < target)
      {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if (c == END_OF_LINE)
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}
