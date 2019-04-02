module top_module(input logic N64_data,
								input logic remote_data,
								input logic [7:0] buttons,
								input logic nes_clk, snes_clk, n64_clk,
								input logic NES_latch, SNES_latch,
								input logic nes_reset, snes_reset,
		  						input logic remote_reset,
								input logic reset_n,	//reset for clock_counter_1MHz
								input logic [1:0] controller_select,
								input logic console_select,
								output logic nes_output[7:0] //nes controller output data line
								output logic snes_output[11:0]
			); 

			logic nes_data_bus;
			logic clk_bus;
			logic clk_4M;
			logic clk_1M;
			logic remote_selection;	//12-bit data line from controller MUX to console DEMUX
			logic [7:0] N64_state_bus;
			
			logic [11:0] to_NES_12;
			logic [7:0] to_NES_8;
			logic [11:0] to_SNES;
			logic [11:0] to_active_controller; 
			
			assign to_NES_8 = to_NES_12[7:0];

			//TODO:
			//4 modules

			//Instance of built in module to select 4.29MHz clock
			OSCH #("4.29") osc_int (
				.STDBY(1'b0),	//Specifies active state
				.OSC(clk_4M),	//Output 4.29MHz to clk_4M net
				.SEDSTDBY());	//Leaves SEDSTDBY pin unconnected

			//Instantiation of 1MHz clock divider (4MHz input, 1MHz output)
			clock_counter_1MHz clk_divider(
				.clk_4MHz(clk_4M),
				.reset_n(reset_n),
				.clk_1MHz(clk_1M));

			//Active Controller selector (MUX)
			active_controller(
				.remote_data(to_active_controller),	//8-bit raw remote data input
			   	.N64_data(N64_state_bus),	//8-bit raw N64 data input
			   	.button_data(buttons),	//8-bit raw button input
			   	.select_bits(controller_select), //2-bit data input
			   	.selection(remote_selection)	//Output 12-bit selection
			);
			

			//Active Console selector (DEMUX)
			active_console console_select(
				.controller_selection(remote_selection),
		      		.select_bit(console_select), //Bit to select console
		      		.NES_console(to_NES_12),	//12-bit data output
	      	      		.SNES_console(to_SNES)	//12-bit data output
			);
			
			shift_register_PISO_8bit NES(
				.clk(nes_clk),
				.reset(nes_reset),
				.load(NES_latch),
				.data(to_NES_8),
				.sout(nes_output)
			);
			
			shift_register_PISO_12bit SNES(
				.clk(snes_clk),
				.reset(snes_reset),
				.load(SNES_latch),
				.data(to_SNES),
				.sout(snes_output)
			);

			//N64 Controller Reader
			n64_button_reader N64_C(
						.clk_4M(clk_4M),
						.clk_1M(clk_1M),
						.n64_data(N64_data),
						.n64_button_state(N64_state_bus),
						.n64_controller_clk(n64_clk)
				);

			//Remote Reader

			remote_translator IR_translator(
				.serial_from_IR(remote_data),
				.OneMHz_clock(clk_1M),
				.reset(remote_reset),
				.to_NES_shift_register(to_active_controler)
			);
endmodule
