#pragma once

enum x86Reg{ rNONE, rEAX, rEBX, rECX, rEDX, rESP, rEDI, rEBP, rESI, };
enum x87Reg{ rST0, rST1, rST2, rST3, rST4, rST5, rST6, rST7, };
enum x86Size{ sNONE, sBYTE, sWORD, sDWORD, sQWORD, };
enum x86Cond{ condO, condNO, condB, condC, condAE, condNB, condNC, condE, condZ, condNE, condNZ,
				condBE, condNA, condA, condNBE, condS, condNS, condP, condPE, condNP, condPO,
				condL, condNGE, condGE, condNL, condLE, condNG, condG, condNLE };

const int rAX = rEAX;
const int rAL = rEAX;
const int rBX = rEBX;
const int rBL = rEBX;

void x86ClearLabels();

int x86FLDZ(unsigned char* stream);
int x86FLD1(unsigned char* stream);

// fld st*
int x86FLD(unsigned char *stream, x87Reg reg);
// fld *word [reg+shift]
int x86FLD(unsigned char *stream, x86Size size, x86Reg reg, unsigned int shift);
// fld *word [regA+regB+shift]
int x86FLD(unsigned char *stream, x86Size size, x86Reg regA, x86Reg regB, int shift);

// fild dword [reg]
int x86FILD(unsigned char *stream, x86Size, x86Reg reg);

// fst *word [reg+shift]
int x86FST(unsigned char *stream, x86Size size, x86Reg reg, unsigned int shift);
// fst *word [regA+regB+shift]
int x86FST(unsigned char *stream, x86Size size, x86Reg regA, x86Reg regB, int shift);

// fstp st*
int x86FSTP(unsigned char *stream, x87Reg dst);
// fstp *word [reg+shift]
int x86FSTP(unsigned char *stream, x86Size size, x86Reg reg, unsigned int shift);
// fstp *word [regA+regB+shift]
int x86FSTP(unsigned char *stream, x86Size size, x86Reg regA, x86Reg regB, int shift);

// fistp *word [reg+shift]
int x86FISTP(unsigned char *stream, x86Size size, x86Reg reg, unsigned int shift);

// fadd *word [reg]
int x86FADD(unsigned char *stream, x86Size size, x86Reg reg);
// faddp
int x86FADDP(unsigned char *stream);

// fsub *word [reg]
int x86FSUB(unsigned char *stream, x86Size size, x86Reg reg);
// fsubr *word [reg]
int x86FSUBR(unsigned char *stream, x86Size size, x86Reg reg);
// fsubp *word [reg]
int x86FSUBP(unsigned char *stream);
// fsubrp *word [reg]
int x86FSUBRP(unsigned char *stream);

// fmul *word [reg]
int x86FMUL(unsigned char *stream, x86Size size, x86Reg reg);
// fmulp
int x86FMULP(unsigned char *stream);

// fdiv *word [reg]
int x86FDIV(unsigned char *stream, x86Size size, x86Reg reg);
// fdivr *word [reg]
int x86FDIVR(unsigned char *stream, x86Size size, x86Reg reg);
// fdivrp
int x86FDIVRP(unsigned char *stream);

int x86FCHS(unsigned char *stream);

int x86FPREM(unsigned char *stream);

int x86FSQRT(unsigned char *stream);

int x86FSINCOS(unsigned char *stream);
int x86FPTAN(unsigned char *stream);

int x86FRNDINT(unsigned char *stream);

// fcomp *word [reg+shift]
int x86FCOMP(unsigned char *stream, x86Size size, x86Reg reg, int shift);

// fnstsw ax
int x86FNSTSW(unsigned char *stream);

// fstcw word [esp]
int x86FSTCW(unsigned char *stream);
// fldcw word [esp+shift]
int x86FLDCW(unsigned char *stream, int shift);

// push *word [regA+regB+shift]
int x86PUSH(unsigned char *stream, x86Size size, x86Reg regA, x86Reg regB, int shift);
int x86PUSH(unsigned char *stream, x86Reg reg);
int x86PUSH(unsigned char *stream, int num);

int x86POP(unsigned char *stream, x86Reg reg);

// mov dst, num
int x86MOV(unsigned char *stream, x86Reg dst, int num);
// mov dst, src
int x86MOV(unsigned char *stream, x86Reg dst, x86Reg src);
// mov dst, dword [src+shift]
int x86MOV(unsigned char *stream, x86Reg dst, x86Reg src, x86Size, int shift);

// mov *word [regA+shift], num
int x86MOV(unsigned char *stream, x86Size size, x86Reg regA, int shift, int num);
// mov *word [regA+regB+shift], src
int x86MOV(unsigned char *stream, x86Size size, x86Reg regA, x86Reg regB, int shift, x86Reg src);

// movsx dst, *word [regA+regB+shift]
int x86MOVSX(unsigned char *stream, x86Reg dst, x86Size size, x86Reg regA, x86Reg regB, int shift);

// lea dst, [src+shift]
int x86LEA(unsigned char *stream, x86Reg dst, x86Reg src, int shift);
// lea dst, [src*multiplier+shift]
int x86LEA(unsigned char *stream, x86Reg dst, x86Reg src, int multiplier, int shift);

// neg dword [reg+shift]
int x86NEG(unsigned char *stream, x86Size, x86Reg reg, int shift);

// add dst, num
int x86ADD(unsigned char *stream, x86Reg dst, int num);
// add dword [reg+shift], op2
int x86ADD(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// adc dst, num
int x86ADC(unsigned char *stream, x86Reg dst, int num);
// adc dword [reg+shift], num
int x86ADC(unsigned char *stream, x86Size, x86Reg reg, int shift, int num);
// adc dword [reg+shift], op2
int x86ADC(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// sub dst, num
int x86SUB(unsigned char *stream, x86Reg dst, int num);
// sub dword [reg+shift], op2
int x86SUB(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// sbb dst, num
int x86SBB(unsigned char *stream, x86Reg dst, int num);
// sbb dword [reg+shift], op2
int x86SBB(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// imul dst, num
int x86IMUL(unsigned char *stream, x86Reg srcdst, int num);
// imul src
int x86IMUL(unsigned char *stream, x86Reg src);

// idiv dword [reg]
int x86IDIV(unsigned char *stream, x86Size, x86Reg reg);

// shl reg, shift
int x86SHL(unsigned char *stream, x86Reg reg, int shift);
// shl dword [reg], shift
int x86SHL(unsigned char *stream, x86Size, x86Reg reg, int shift);

// sal eax, cl
int x86SAL(unsigned char *stream);
// sar eax, cl
int x86SAR(unsigned char *stream);

// not dword [reg+shift]
int x86NOT(unsigned char *stream, x86Size, x86Reg reg, int shift);

// and dword [reg+shift], op2
int x86AND(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// or op1, op2
int x86OR(unsigned char *stream, x86Reg op1, x86Reg op2);
// or op1, dword [reg+shift]
int x86OR(unsigned char *stream, x86Reg op1, x86Size, x86Reg reg, int shift);

// xor op1, op2
int x86XOR(unsigned char *stream, x86Reg op1, x86Reg op2);
// xor dword [reg+shift], op2
int x86XOR(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

// cmp reg, num
int x86CMP(unsigned char *stream, x86Reg reg, int num);
// cmp reg1, reg2
int x86CMP(unsigned char *stream, x86Reg reg1, x86Reg reg2);
// cmp dword [reg+shift], op2
int x86CMP(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);
// cmp dword [reg+shift], num
int x86CMP(unsigned char *stream, x86Size, x86Reg reg, int shift, int op2);

int x86TEST(unsigned char *stream, x86Reg op1, x86Reg op2);
// test ah, num
int x86TESTah(unsigned char* stream, char num);

// xchg dword [reg], op2
int x86XCHG(unsigned char *stream, x86Size, x86Reg reg, int shift, x86Reg op2);

int x86CDQ(unsigned char *stream);

// setcc cl
int x86SETcc(unsigned char *stream, x86Cond cond);

int x86CALL(unsigned char *stream, x86Reg address);
int x86CALL(unsigned char *stream, const char* label);
int x86RET(unsigned char *stream);

int x86REP_MOVSD(unsigned char *stream);

int x86INT(unsigned char *stream, int interrupt);

int x86NOP(unsigned char *stream);

int x86Jcc(unsigned char *stream, const char* label, x86Cond cond, bool isNear);
int x86JMP(unsigned char *stream, const char* label, bool isNear);

void x86AddLabel(unsigned char *stream, const char* label);

//int x86(unsigned char *stream);