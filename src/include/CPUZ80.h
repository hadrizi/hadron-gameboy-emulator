#pragma once
#define CLOCKSPEED 4194304 // 4.194304 MHz as stated in the technical documentaion

#include <iostream>
#include <vector>
#include <string>
#include <map>

#include "core.h"
#include "Screen.h"

class GameBoy;

class CPUZ80
{
public:
	CPUZ80();
	~CPUZ80();

	// This function is used to build interface to GAmeBoy itself and get access to other hardware
	// like screen or memory
	void connect_device(GameBoy* instance);

	// This function is not used in emulation< it is just for debugging purpose
	// Constructs a map of command strings
	std::map<H_WORD, std::string> disassemble(H_WORD, H_WORD);

	// One CPU clock
	void cpu_clock();
	
	// Resets CPU according to documentation
	void reset(); 

	// Checks if all desired cycles passed
	bool complete();
	
	// Direct Memory Access Transfer
	void DMA(H_BYTE);
public:
	/*
		FLAGS
		Flag register(F) consists of the following bits
		7 6 5 4 3 2 1 0
		Z N H C 0 0 0 0

		Zero Flag (Z):
		This bit is set when the result of a math operation
		is zero or two values match when using the CP
		instruction.

		Subtract Flag (N):
		This bit is set if a subtraction was performed in the
		last math instruction.

		Half Carry Flag (H):
		This bit is set if a carry occurred from the lower
		nibble in the last math operation.

		Carry Flag (C):
		This bit is set if a carry occurred from the last
		math operation or if register A is the smaller value
		when executing the CP instruction.

		As Game Boy CPU is not the exact replica of Zilog 80 CPU
		it only has 4 flags, when Z80 had 8 of them
	*/
	enum FLAGS
	{
		Z = (1 << 7), // Zero Flag
		N = (1 << 6), // Subtract Flag
		H = (1 << 5), // Half Carry Flag
		C = (1 << 4)  // Carry Flag
	};

	/*
		Z80 CPU has eight 8-bit registers and two 16-bit registers
		However some instructions allows to use them as 16-bit registers in pairing

		16bit Hi   Lo   Name/Function
		AF    A    F    Accumulator & Flags
		BC    B    C    BC
		DE    D    E    DE
		HL    H    L    HL
		SP    -    -    Stack Pointer
		PC    -    -    Program Counter/Pointer

		Register structure uses union to imitate this behavior
	*/

	Register AF = 0x0000;
	Register BC = 0x0000;
	Register DE = 0x0000;
	Register HL = 0x0000;

	Register PC = 0x0000; // Program Counter - points to the next instruction to be executed
	Register SP = 0x0000; // Stack Pointer - points to the current stack position

	// SPECIAL REGISTERS
	/*
		In most cases these boys are stored in the memory and they are not registers
		but pointers to the specific memory address
	*/
	/*
		Interrupts.
		Above registers were interupts related so below I list all interupts in their priority order

		Bit 0: V-Blank Interupt
		Bit 1: LCD Interupt
		Bit 2: Timer Interupt
		Bit 4: Joypad Interupt

		The lower the bit the higher priority that interupt has,
		so V-Blank has the highest priority meaning if this interupt and another interupt are both requested
		in the Interupt Request Register then V-Blank will be serviced first.
	*/
	bool    PEI = false;  // Pending Enable Interupts
	bool    PDI = false;  // Pending Disable Interupts
	bool    IME = true;   // Interupt Master Enabled. This is one is neither register nor a memory pointer, 
						  // it just says if registers are enabled or not
	H_BYTE* IE = nullptr; // Interupt Enable. Points to $FFFF. Determines which interupts are allowed
	H_BYTE* IF = nullptr; // Interupt Request. Points to $FF0F. Determines which interupts are requested
	/*
		Timer
		GameBoy has internal timer. It has nothing to do with CPU clock!!!
		This timer consists of four parts
		1. Timer itself     - TIMA at 0xFF05 memory address
		2. Timer modula     - TMA  at 0xFF06 memory address
		3. Timer controller - TAC  at 0xFF07 memory address
		4. Divider			- DIV  at 0xFF04 memory address
		
		TAC determines if timer is enabled and with which frequency it updates
			Bit 2 - Timer Enable
					0: Disabled
					1: Enabled
			Bit 1+0 - Frequency
					00: 4096    HZ - every 1024 CPU cycles
					01: 2621444 HZ - every 16   CPU cycles
					10: 65536   HZ - every 64   CPU cycles
					11: 16384   HZ - every 256  CPU cycles

		TIMA increments every frequency cycle and goes from $00 to $FF
		and when it overflows it resets to the value stored at TMA
		and requests Timer(0x04) interrupt.

		Divider is simmilar to TIMA it goes from $00 to $FF, too.
		However, it increments at fixed rate of 16384 Hz(every 256 CPU cycles).
		If overflow happens it resets to zero. When CPU tries to write data to
		DIV address it sets it to zero.
	*/
	struct
	{
		H_BYTE* TIMA  = nullptr; // FF05
		H_BYTE* TMA   = nullptr; // FF06
		H_BYTE* TAC   = nullptr; // FF07
		bool overflow = false;
		int frequency = 1024;

		inline void reset() { (*TIMA) = (*TMA); }
	} clock;
	H_BYTE* DIV = nullptr; // FF04

	/*
		LCD

		If (*LY) is 144 it is time to VBlank(0x00) interrupt
	*/
	struct {
		const int scanlines = 153;
		const int invisible_scanlines = 9;
		const int frequency = 456;

		H_BYTE* LY   = nullptr; // FF44. Read operations sets to zero
		H_BYTE* LYC  = nullptr; // FF45
		H_BYTE* STAT = nullptr; // FF41. LCD status
								// Bits 6-3 - Interrupt section, 6th bit is LYC=LY
								// Bit 5 - Mode 10
								// Bit 4 - Mode 01
								// Bit 3 - Mode 00
								//		0: Non Selection
								//		1: Selection
								// Bit 2 - Coincidence Flag
								//		0: LYC != LY
								//		1: LYC == LY
								// Bit 1-0 - Mode Flag
								//		00: H-Blank
								//		01: V-Blank
								//		10: Search RAM for Sprites
								//		11: Transfering data to LCD driver
		H_BYTE* LCDC = nullptr; // FF40. $91 on reset
								// Bit 7: Control mode
								//		 0: Disable
								//		 1: Enable
								// Bit 6: Window tile map display select
								//		 0: &9800-$9BFF
								//		 1: &9C00-$9FFF
								// Bit 5: Window display
								//		 0: off
								//		 1: on
								// Bit 4: Backgound and Window Tile data select
								//		 0: $8800-$97FF
								//		 1: $8000-$8FFF <- Same area as OBJ
								// Bit 3: Backgound tile map display select
								//		 0: $9800-$9BFF
								//		 1: $9C00-$9FFF
								// Bit 2: Sprite(OBJ) size (WIDTH x HEIGHT)
								//		 0: 8 x 8
								//		 1: 8 x 16
								// Bit 1: Sprite(OBJ) dislay
								//		 0: off
								//		 1: on
								// Bit 0: Backgound and Window display
								//		 0: off
								//		 1: on
		H_BYTE* SCY = nullptr;  // FF42. Scroll Y
								// Background Y screen position
		H_BYTE* SCX = nullptr;  // FF43. Scroll X
								// Background X screen position
		H_BYTE* WY  = nullptr;  // FF4A. Window Y position
								// 0 <= WY <= 143
		H_BYTE* WX  = nullptr;  // FF4B. Scroll X
								// 0 <= WY <= 166

		Screen* s = nullptr;
		inline bool enabled() { return ((*LCDC) & (1u << 7)) > 0 ? true : false; }
		inline void reset() { (*LY) = 0; (*STAT) &= 0xFC; (*STAT) |= 1 << 0; }
	} LCD;

private:
	// Interrupt bits
	enum
	{
		INT_VBlank = 0, // Bit 0
		INT_LCD    = 1, // Bit 1
		INT_Timer  = 2, // Bit 2
		INT_Serial = 3, // Bit 3
		INT_Joypad = 4, // Bit 4
	};

	H_BYTE* fetched8_ptr  = nullptr; // This custom register is used by Data Functions to store 8-bit fetched data
	H_WORD* fetched16_ptr = nullptr; // This custom register is used by Data Functions to store 16-bit fetched data
	H_WORD  temp		  = 0x0000;  // A buffer register. Just for case
	H_BYTE  opcode        = 0x00;    // Instruction byte
	H_BYTE  cycles        = 0;	     // Counts how many cycles has remaining
	
	// Counters are not a part of CPU, their purpose is to track cycles
	// for different parts of emulatoin so CPU and other hadrdware
	// work synchronous
	struct {
		H_DWORD clock_count    = 0; // Counts how many cycles have passed. Emulator doesn't use it's only for debugging
		H_WORD  timer_count    = 0; // Counts how many cycles have passed. Used to proceed timer
		H_WORD  divider_count  = 0; // Counts how many cycles have passed. Used to proceed divider
		H_WORD  scanline_count = 0; // Counts how many cycles have passed. Used to draw LCD and determine its mode
	
		inline void inc()
		{
			clock_count++;
			timer_count++;
			divider_count++;
			scanline_count++;
		}

		inline void reset()
		{
			clock_count = 0;
			timer_count = 0;
			divider_count = 0;
			scanline_count = 0;
		}

		inline void log()
		{
			std::cout << "Global count >> " << clock_count << std::endl
				<< "Timer count >> " << timer_count << std::endl
				<< "Divider count >> " << divider_count << std::endl
				<< "Scanline count >> " << scanline_count << std::endl
				<< std::endl;
		}
	} counters;

	// GameBoy instance
	GameBoy* gb = nullptr;
	void    write(H_WORD, H_BYTE);
	H_BYTE  read(H_WORD);
	H_BYTE* read_ptr(H_WORD);
	void    write(Register, H_BYTE);
	H_BYTE  read(Register);
	H_BYTE* read_ptr(Register);

	struct INSTRUCTION
	{
		std::string name;
		void (CPUZ80::* op_func)(void) = nullptr;
		void (CPUZ80::* data_func)(void) = nullptr;
		H_BYTE cycles = 0;
	};
	std::vector<INSTRUCTION> opcodes;
	std::vector<INSTRUCTION> prefixes;

private:
	// Functions to manipulate F register
	H_BYTE get_flag(FLAGS);
	void set_flag(FLAGS, bool);
	void reset_flag(FLAGS);

	// Update Functions
	void update_timers();
	void update_LCD();

	inline void inc_PC(int k = 1) { PC = PC + 1 * k; }
	inline void inc_SP(int k = 1) { SP = SP + 1 * k; }
	inline void dec_SP(int k = 1) { SP = SP - 1 * k; }

	/*
		CPU Functions
		These functions are not aprt of the original Game Boy CPU
		and they don't have opcodes
		I implemented them just to sophisticate realization of opcodes

		TODO add templates
	*/

	void CPU_REG_LOAD   (Register&, H_WORD);			 // Loads data to register
	void CPU_MEM_LOAD   (H_WORD, H_BYTE);				 // Loads data to memory
	void CPU_MEM_LOAD   (Register&, H_BYTE);			 // Loads data to memory
	void CPU_ACC_ADD    (H_BYTE);					     // Adds data to accumulator register(A)
	void CPU_HL_ADD     (H_WORD);					     // Adds data to HL register(16-bit)
	void CPU_SP_ADD	    (H_BYTE);					     // Adds data to SP register(8-bit)
	void CPU_ACC_SUB    (H_BYTE, bool compare = false);  // Subtracts data from accumulator register(A). It is also compare function
	void CPU_ACC_AND    (H_BYTE);					     // Bitwise AND with accumulator register(A)
	void CPU_ACC_OR     (H_BYTE);					     // Bitwise OR with accumulator register(A)
	void CPU_ACC_XOR    (H_BYTE);					     // Bitwise XOR with accumulator register(A)
	void CPU_ACC_FLIP   ();								 // Flips all bits of accumulator register(A)
	void CPU_16REG_INC  (H_WORD*);						 // Increments 16-bit register
	void CPU_8REG_INC   (H_BYTE*);				         // Increments 8-bit register
	void CPU_16REG_DEC  (H_WORD*);						 // Decrements 16-bit register
	void CPU_8REG_DEC   (H_BYTE*);				         // Decrements 8-bit register
	void CPU_8REG_SWAP  (H_BYTE*);				         // Swaps 8-bit register nibbles
	void CPU_16REG_SWAP (H_WORD*);				         // Swaps 16-bit register nibbles. Not used
	bool CPU_TEST_BIT   (H_BYTE,  size_t);		         // Checks if specific bit set
	void CPU_SET_BIT    (H_BYTE*, size_t);		         // Sets specific bit
	void CPU_RESET_BIT  (H_BYTE*, size_t);		         // Resets specific bit
	void CPU_8REG_RL    (H_BYTE*);				         // Rotates 8-bit register left
	void CPU_8REG_RLC   (H_BYTE*);				         // Rotates 8-bit register left. Old bit 7 to C flag 
	void CPU_8REG_RR    (H_BYTE*);				         // Rotates 8-bit register right
	void CPU_8REG_RRC   (H_BYTE*);				         // Rotates 8-bit register right. Old bit 0 to C flag
	void CPU_8REG_SLA   (H_BYTE*);				         // Shifts arithmetically 8-bit register left. Old bit 7 to C flag
	void CPU_8REG_SRA   (H_BYTE*);				         // Shifts arithmetically 8-bit register right. Old bit 0 to C flag 
	void CPU_8REG_SRL   (H_BYTE*);				         // Shifts logically 8-bit register right. MSB set to 0. Old bit 7 to C flag 
	void CPU_PUSH_8     (H_BYTE);				         // Pushes 8-bit value onto stack. SP is decremented once 
	void CPU_PUSH_16    (H_WORD);				         // Pushes 16-bit value onto stack. SP is decremented twice 
	void CPU_CALL	    (H_WORD);						 // Pushes PC to the stack. And jumps to specific address 
	H_BYTE CPU_POP_8    ();								 // Pops 8-bit value from stack. SP is incremented once
	H_WORD CPU_POP_16   ();								 // Pops 16-bit value from stack. SP is incremented twice 
	void CPU_PERFORM_INT();								 // Performs interupts. Checks if any interupts is requsted and services them if true
	void CPU_REQUEST_INT(size_t);						 // Requests interup. Sets specific bit in IF
	void CPU_PENDING_IME();								 // Checks pending IME switch
	void CPU_CLOCK_INCREMENT();							 // Increments clock
	void CPU_TIMER_INCREMENT();							 // Increments timer
	void CPU_TIMER_CHECK();							     // Checks timer for overflow
	H_BYTE CPU_TIMER_BIT();							     // Returns TAC frequency bit
	void CPU_TIMER_FREQ();								 // Sets timer frequency
	void CPU_DIVIDER_INCREMENT();						 // Increments divider

	/*
		LCD Functions
		These functions provides interface to manipulate the screen	
	*/
	
	void LCD_SET_STATUS();	                  // Updates LCD status according to current cycle
	void LCD_DRAW_LINE();	                  // Renders current line
	void LCD_RENDER_TILES();                  // Renders current line
	void LCD_RENDER_SPRITES();                // Renders current line
	ScreenData LCD_GET_COLOR(H_BYTE, H_WORD); // Returns color according to the palette

	/* 
		Data Functions
		I didn't know how to implement parameters functionality
		so I came up with the idea of Data Functions.

		They are pretty similar to NES Address Modes.
		They take data from specific source and put its reference to the fetched pointer register 
		or to the temp register
	*/

	// 8-bit Data Functions============================================================================

	void dimm_8();	// Immediate 8-bit data function. It fetches data from (PC)
	void da();		// A register data function. It fetches A register
	void db();		// B register data function. It fetches B register
	void dc();		// C register data function. It fetches C register
	void dd();		// D register data function. It fetches D register
	void de();		// E register data function. It fetches E register
	void dh();		// H register data function. It fetches H register
	void dl();		// L register data function. It fetches L register

	void b0();		// Bit data function. It fetches 00000001b
	void b1();		// Bit data function. It fetches 00000010b
	void b2();		// Bit data function. It fetches 00000100b
	void b3();		// Bit data function. It fetches 00001000b
	void b4();		// Bit data function. It fetches 00010000b
	void b5();		// Bit data function. It fetches 00100000b
	void b6();		// Bit data function. It fetches 01000000b
	void b7();		// Bit data function. It fetches 10000000b

	void mimm_16(); // Immidiate memory address data function. It fetches data from (nn)
	void mbc();		// BC memory address data function. It fetches data from (BC)
	void mde();		// DE memory address data function. It fetches data from (DE)
	void mhl();		// HL memory address data function. It fetches data from (HL)
	void mFF00c();	// Specific  memory address data function. It fetches data from (0xFF00 + C)
	void mFF00n();	// Immidiate specific memory address data function. It takes fetches from (0xFF00 + n)

	// =================================================================================================
	
	// 16-bit Data Functions============================================================================

	void dimm_16(); // Immediate 16-bit data function. It fetches data from ((PC) << 8) | (PC + 1))
	void daf();		// AF register data function. It fetches AF register
	void dbc();		// BC register data function. It fetches BC register
	void dde();		// DE register data function. It fetches DE register
	void dhl();		// HL register data function. It fetches HL register
	void dsp(); 	// SP register data function. It fetches SP
	void dspn();	// SP register data function. It fetches SP+n

	//void sp_16();	// Stack data function. It fetches data from ((SP) << 8) | (SP + 1))

	// =================================================================================================

	// Misc Data Functions==============================================================================
	
	void dnop(); // Empty data function. It fetches nothing
	
	// =================================================================================================

	// Opcodes Instructions=============================================================================

	// Load instructions
	void LD_A();   void LD_B();   void LD_C();   void LD_D(); 
	void LD_E();   void LD_H();   void LD_L();   void LDD_A();
	void LDI_A();  void LD_BC();  void LD_DE();  void LD_HL();	
	void LD_SP();  void LDHL();   void PUSH();	 void POP();
	
	void LD_M_BC();    void LD_M_DE();  void LD_M_HL();  void LD_M_NN(); 
	void LD_M_FFFOC(); void LDD_M_HL(); void LDI_M_HL(); void LDH_M();

	// ALU instructions
	void ADD_A();  void ADC();    void SUB();    void SBC(); 
	void AND();    void OR();     void XOR();    void CP();
	void INC_8();  void DEC_8();  void ADD_HL(); void ADD_SP();
	void INC_16(); void DEC_16();

	// Misc instructions
	void SWAP(); void DAA(); void CPL();    void CCF();
	void SCF();  void NOP(); void HALT();   void STOP();
	void DI();   void EI();  void PREFIX();

	// Rotate and Shift instructions
	void RLCA(); void RLA(); void RRCA(); void RRA();
	void RLC();  void RL();  void RRC();  void RR();
	void SLA();  void SRA(); void SRL();

	// Bit instructions
	void BIT_A(); void BIT_B(); void BIT_C(); void BIT_D();
	void BIT_E(); void BIT_H(); void BIT_L(); void BIT_M_HL();

	void SET_A(); void SET_B(); void SET_C(); void SET_D();
	void SET_E(); void SET_H(); void SET_L(); void SET_M_HL();

	void RES_A(); void RES_B(); void RES_C(); void RES_D();
	void RES_E(); void RES_H(); void RES_L(); void RES_M_HL();

	// Jump instructions
	void JP();   void JPNZ(); void JPZ();  void JPNC();
	void JPC();  void JR();   void JRNZ(); void JRZ();
	void JRNC(); void JRC();

	// Call instructions
	void CALL();   void CALL_NZ(); void CALL_Z(); void CALL_NC();
	void CALL_C();

	// Restart instructions
	void RST_00(); void RST_08(); void RST_10(); void RST_18();
	void RST_20(); void RST_28(); void RST_30(); void RST_38();

	// Return instructions
	void RET();   void RET_NZ(); void RET_Z(); void RET_NC();
	void RET_C(); void RETI();
	
	// Unknown instruction
	void XXX();

	// =================================================================================================
};

