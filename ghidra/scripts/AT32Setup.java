// Prepare a raw AT32F403A/407 (Cortex-M4F) flash image for analysis:
// memory map (SRAM, boot ROM, XMC, peripherals), flash block permissions,
// and the vector table with CMSIS handler names as functions.
// Expects the application image imported with the Raw Binary loader at
// 0x08002C00 (after the 11 KiB bootloader) and language ARM:LE:32:Cortex. Run as a preScript so auto-analysis starts
// from the vector table entries; it is idempotent and can be re-run.
// Port of firmware/scripts/setup-artery-cortex-m-memory-map.py (Jython is
// no longer bundled with Ghidra 12).
//@category RT950
import java.util.HashMap;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

public class AT32Setup extends GhidraScript {

	static final long FLASH_START = 0x08000000L;
	static final long APP_BASE = 0x08002C00L;
	// Image offsets 0x400-0x7FF hold the fwcrypt key block; fwcrypt_io.py does
	// not decrypt it, so the bytes there are not the real code.
	static final long KEY_BLOCK_OFF = 0x400L, KEY_BLOCK_LEN = 0x400L;

	// name, start, end, perms
	static final Object[][] REGIONS = {
		{ "SRAM", 0x20000000L, 0x20017FFFL, "RW" },
		{ "BOOTROM", 0x1FFFB000L, 0x1FFFEFFFL, "R" },
		{ "USD", 0x1FFFF800L, 0x1FFFF8FFL, "R" },
		{ "XMC_MEM", 0x60000000L, 0x9FFFFFFFL, "RWV" },
		{ "XMC_REG", 0xA0000000L, 0xA0000FFFL, "RWV" },
		{ "SCS", 0xE000E000L, 0xE000EFFFL, "RWV" },
		// APB1
		{ "TMR2", 0x40000000L, 0x400003FFL, "RWV" },
		{ "TMR3", 0x40000400L, 0x400007FFL, "RWV" },
		{ "TMR4", 0x40000800L, 0x40000BFFL, "RWV" },
		{ "TMR5", 0x40000C00L, 0x40000FFFL, "RWV" },
		{ "TMR6", 0x40001000L, 0x400013FFL, "RWV" },
		{ "TMR7", 0x40001400L, 0x400017FFL, "RWV" },
		{ "TMR12", 0x40001800L, 0x40001BFFL, "RWV" },
		{ "TMR13", 0x40001C00L, 0x40001FFFL, "RWV" },
		{ "TMR14", 0x40002000L, 0x400023FFL, "RWV" },
		{ "RTC", 0x40002800L, 0x40002BFFL, "RWV" },
		{ "WWDT", 0x40002C00L, 0x40002FFFL, "RWV" },
		{ "WDT", 0x40003000L, 0x400033FFL, "RWV" },
		{ "SPI2_I2S2", 0x40003800L, 0x40003BFFL, "RWV" },
		{ "SPI3_I2S3", 0x40003C00L, 0x40003FFFL, "RWV" },
		{ "USART2", 0x40004400L, 0x400047FFL, "RWV" },
		{ "USART3", 0x40004800L, 0x40004BFFL, "RWV" },
		{ "UART4", 0x40004C00L, 0x40004FFFL, "RWV" },
		{ "UART5", 0x40005000L, 0x400053FFL, "RWV" },
		{ "I2C1", 0x40005400L, 0x400057FFL, "RWV" },
		{ "I2C2", 0x40005800L, 0x40005BFFL, "RWV" },
		{ "USBFS", 0x40005C00L, 0x40005FFFL, "RWV" },
		{ "USBFS_SRAM", 0x40006000L, 0x400063FFL, "RWV" },
		{ "CAN1", 0x40006400L, 0x400067FFL, "RWV" },
		{ "CAN2", 0x40006800L, 0x40006BFFL, "RWV" },
		{ "BPR", 0x40006C00L, 0x40006FFFL, "RWV" },
		{ "PWC", 0x40007000L, 0x400073FFL, "RWV" },
		{ "DAC", 0x40007400L, 0x400077FFL, "RWV" },
		{ "USBFS_SRAM_EXT", 0x40007800L, 0x400083FFL, "RWV" },
		// APB2
		{ "IOMUX", 0x40010000L, 0x400103FFL, "RWV" },
		{ "EXINT", 0x40010400L, 0x400107FFL, "RWV" },
		{ "GPIOA", 0x40010800L, 0x40010BFFL, "RWV" },
		{ "GPIOB", 0x40010C00L, 0x40010FFFL, "RWV" },
		{ "GPIOC", 0x40011000L, 0x400113FFL, "RWV" },
		{ "GPIOD", 0x40011400L, 0x400117FFL, "RWV" },
		{ "GPIOE", 0x40011800L, 0x40011BFFL, "RWV" },
		{ "ADC1", 0x40012400L, 0x400127FFL, "RWV" },
		{ "ADC2", 0x40012800L, 0x40012BFFL, "RWV" },
		{ "TMR1", 0x40012C00L, 0x40012FFFL, "RWV" },
		{ "SPI1_I2S1", 0x40013000L, 0x400133FFL, "RWV" },
		{ "TMR8", 0x40013400L, 0x400137FFL, "RWV" },
		{ "USART1", 0x40013800L, 0x40013BFFL, "RWV" },
		{ "ADC3", 0x40013C00L, 0x40013FFFL, "RWV" },
		{ "TMR9", 0x40014C00L, 0x40014FFFL, "RWV" },
		{ "TMR10", 0x40015000L, 0x400153FFL, "RWV" },
		{ "TMR11", 0x40015400L, 0x400157FFL, "RWV" },
		{ "ACC", 0x40015800L, 0x40015BFFL, "RWV" },
		{ "I2C3", 0x40015C00L, 0x40015FFFL, "RWV" },
		{ "USART6", 0x40016000L, 0x400163FFL, "RWV" },
		{ "UART7", 0x40016400L, 0x400167FFL, "RWV" },
		{ "UART8", 0x40016800L, 0x40016BFFL, "RWV" },
		{ "SDIO1", 0x40018000L, 0x400183FFL, "RWV" },
		// AHB
		{ "DMA1", 0x40020000L, 0x400203FFL, "RWV" },
		{ "DMA2", 0x40020400L, 0x400207FFL, "RWV" },
		{ "CRM", 0x40021000L, 0x400213FFL, "RWV" },
		{ "FLASH_IF", 0x40022000L, 0x400223FFL, "RWV" },
		{ "CRC", 0x40023000L, 0x400233FFL, "RWV" },
		{ "SDIO2", 0x40023400L, 0x400237FFL, "RWV" },
		{ "EMAC", 0x40028000L, 0x40029FFFL, "RWV" },
	};

	// From libraries/cmsis/.../startup/gcc/startup_at32f403a_407.s
	static final String[] VECTORS = {
		null, "Reset_Handler", "NMI_Handler", "HardFault_Handler", "MemManage_Handler",
		"BusFault_Handler", "UsageFault_Handler", null, null, null, null, "SVC_Handler",
		"DebugMon_Handler", null, "PendSV_Handler", "SysTick_Handler", "WWDT_IRQHandler",
		"PVM_IRQHandler", "TAMPER_IRQHandler", "RTC_IRQHandler", "FLASH_IRQHandler",
		"CRM_IRQHandler", "EXINT0_IRQHandler", "EXINT1_IRQHandler", "EXINT2_IRQHandler",
		"EXINT3_IRQHandler", "EXINT4_IRQHandler", "DMA1_Channel1_IRQHandler",
		"DMA1_Channel2_IRQHandler", "DMA1_Channel3_IRQHandler", "DMA1_Channel4_IRQHandler",
		"DMA1_Channel5_IRQHandler", "DMA1_Channel6_IRQHandler", "DMA1_Channel7_IRQHandler",
		"ADC1_2_IRQHandler", "USBFS_H_CAN1_TX_IRQHandler", "USBFS_L_CAN1_RX0_IRQHandler",
		"CAN1_RX1_IRQHandler", "CAN1_SE_IRQHandler", "EXINT9_5_IRQHandler",
		"TMR1_BRK_TMR9_IRQHandler", "TMR1_OVF_TMR10_IRQHandler",
		"TMR1_TRG_HALL_TMR11_IRQHandler", "TMR1_CH_IRQHandler", "TMR2_GLOBAL_IRQHandler",
		"TMR3_GLOBAL_IRQHandler", "TMR4_GLOBAL_IRQHandler", "I2C1_EVT_IRQHandler",
		"I2C1_ERR_IRQHandler", "I2C2_EVT_IRQHandler", "I2C2_ERR_IRQHandler", "SPI1_IRQHandler",
		"SPI2_I2S2EXT_IRQHandler", "USART1_IRQHandler", "USART2_IRQHandler",
		"USART3_IRQHandler", "EXINT15_10_IRQHandler", "RTCAlarm_IRQHandler",
		"USBFSWakeUp_IRQHandler", "TMR8_BRK_TMR12_IRQHandler", "TMR8_OVF_TMR13_IRQHandler",
		"TMR8_TRG_HALL_TMR14_IRQHandler", "TMR8_CH_IRQHandler", "ADC3_IRQHandler",
		"XMC_IRQHandler", "SDIO1_IRQHandler", "TMR5_GLOBAL_IRQHandler",
		"SPI3_I2S3EXT_IRQHandler", "UART4_IRQHandler", "UART5_IRQHandler",
		"TMR6_GLOBAL_IRQHandler", "TMR7_GLOBAL_IRQHandler", "DMA2_Channel1_IRQHandler",
		"DMA2_Channel2_IRQHandler", "DMA2_Channel3_IRQHandler", "DMA2_Channel4_5_IRQHandler",
		"SDIO2_IRQHandler", "I2C3_EVT_IRQHandler", "I2C3_ERR_IRQHandler", "SPI4_IRQHandler",
		null, null, null, null, "CAN2_TX_IRQHandler", "CAN2_RX0_IRQHandler",
		"CAN2_RX1_IRQHandler", "CAN2_SE_IRQHandler", "ACC_IRQHandler",
		"USBFS_MAPH_IRQHandler", "USBFS_MAPL_IRQHandler", "DMA2_Channel6_7_IRQHandler",
		"USART6_IRQHandler", "UART7_IRQHandler", "UART8_IRQHandler", "EMAC_IRQHandler",
		"EMAC_WKUP_IRQHandler",
	};

	private Address A(long off) {
		return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(off);
	}

	private void region(Memory mem, String name, long start, long end, String perms) throws Exception {
		Address s = A(start), e = A(end);
		if (mem.intersects(s, e)) {
			return;
		}
		MemoryBlock b = mem.createUninitializedBlock(name, s, end - start + 1, false);
		b.setPermissions(true, perms.contains("W"), perms.contains("X"));
		b.setVolatile(perms.contains("V"));
	}

	@Override
	public void run() throws Exception {
		if (!currentProgram.getLanguageID().getIdAsString().startsWith("ARM:LE:32:Cortex")) {
			printerr("Expected language ARM:LE:32:Cortex, got " + currentProgram.getLanguageID());
		}
		Memory mem = currentProgram.getMemory();
		MemoryBlock flash = mem.getBlock(A(APP_BASE));
		if (flash == null || !flash.isInitialized()) {
			throw new IllegalStateException("No image bytes at 0x08002C00: import with -loader BinaryLoader -loader-baseAddr 0x08002C00");
		}
		long flashEnd = flash.getEnd().getOffset();
		// Read-only flash lets the decompiler fold constants from literal pools.
		flash.setName("FLASH");
		flash.setPermissions(true, false, true);

		Address keyStart = A(APP_BASE + KEY_BLOCK_OFF);
		Address keyEnd = keyStart.add(KEY_BLOCK_LEN - 1);
		if (flash.contains(keyEnd) && mem.getBlock(keyStart).isInitialized()) {
			mem.split(flash, keyStart);
			MemoryBlock key = mem.getBlock(keyStart);
			mem.split(key, keyEnd.add(1));
			key = mem.getBlock(keyStart);
			key.setName("FWCRYPT_KEY");
			key.setPermissions(true, false, false);
			mem.convertToUninitialized(key);
			mem.getBlock(keyEnd.add(1)).setName("FLASH");
			setPlateComment(keyStart, "fwcrypt key block (image offset 0x400-0x7FF), not decrypted by\n"
				+ "fwcrypt_io.py. The real code here (including Reset_Handler) is unknown.");
		}
		region(mem, "BOOTLOADER", FLASH_START, APP_BASE - 1, "RX");

		for (Object[] r : REGIONS) {
			region(mem, (String) r[0], (Long) r[1], (Long) r[2], (String) r[3]);
		}

		// Vector table: word pointers with CMSIS names, handlers become functions.
		Map<Long, String> handlers = new HashMap<>();
		int used = 0;
		for (int i = 0; i < VECTORS.length; i++) {
			Address slot = A(APP_BASE + 4L * i);
			long v = mem.getInt(slot) & 0xFFFFFFFFL;
			clearListing(slot, slot.add(3));
			createData(slot, PointerDataType.dataType);
			if (i == 0) {
				createLabel(slot, "vector_table", true, SourceType.ANALYSIS);
				println(String.format("Initial SP 0x%08x", v));
				continue;
			}
			if (VECTORS[i] == null || v == 0 || v < APP_BASE || v > flashEnd) {
				continue;
			}
			long target = v & ~1L;
			if (!mem.getBlock(A(target)).isInitialized()) {
				// Points into the undecrypted key block: label only, no code.
				createLabel(A(target), VECTORS[i], true, SourceType.ANALYSIS);
				continue;
			}
			// Several unused vectors usually share one weak default handler.
			String name = handlers.containsKey(target) ? "Default_Handler" : VECTORS[i];
			handlers.put(target, name);
			used++;
		}
		for (Map.Entry<Long, String> h : handlers.entrySet()) {
			Address a = A(h.getKey());
			Function f = getFunctionAt(a);
			if (f == null) {
				disassemble(a);
				f = createFunction(a, h.getValue());
			}
			if (f != null) {
				f.setName(h.getValue(), SourceType.ANALYSIS);
			}
			currentProgram.getSymbolTable().addExternalEntryPoint(a);
		}
		println("Vector table: " + used + " populated slots, " + handlers.size() + " distinct handlers");
	}
}
