// Export user-defined function names, labels and comments of the current
// program to a CSV that ApplySymbols.java can re-import.
// Script args: <out.csv> [rebaseFrom] [rebaseTo]
//   rebaseFrom/rebaseTo (hex) shift every address, e.g. 0x80000000 0x08000000
//   to migrate names out of an image that was imported at the wrong base.
//@category RT950
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class ExportSymbols extends GhidraScript {

	private long from = 0, to = 0;

	private String addr(Address a) {
		return String.format("0x%08x", a.getOffset() - from + to);
	}

	private static String q(String s) {
		if (s == null) {
			return "";
		}
		return "\"" + s.replace("\"", "\"\"").replace("\r", "").replace("\n", "\\n") + "\"";
	}

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			throw new IllegalArgumentException("usage: ExportSymbols.java <out.csv> [rebaseFrom rebaseTo]");
		}
		if (args.length >= 3) {
			from = Long.decode(args[1]);
			to = Long.decode(args[2]);
		}
		Listing listing = currentProgram.getListing();
		int n = 0;
		try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
			w.println("address,kind,name,comment");
			for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
				if (s.getSource() != SourceType.USER_DEFINED || s.isExternal()) {
					continue;
				}
				SymbolType t = s.getSymbolType();
				String kind;
				String comment = null;
				if (t == SymbolType.FUNCTION) {
					kind = "function";
					Function f = (Function) s.getObject();
					comment = f.getComment();
				}
				else if (t == SymbolType.LABEL) {
					kind = "label";
				}
				else {
					continue;
				}
				w.println(addr(s.getAddress()) + "," + kind + "," + q(s.getName()) + "," + q(comment));
				n++;
			}
			// Plate/pre comments on addresses that carry no user symbol.
			for (Address a : listing.getCommentAddressIterator(currentProgram.getMemory(), true)) {
				for (CommentType ct : new CommentType[] { CommentType.PLATE, CommentType.PRE, CommentType.EOL }) {
					String c = listing.getComment(ct, a);
					if (c != null) {
						w.println(addr(a) + ",comment_" + ct.name().toLowerCase() + ",," + q(c));
						n++;
					}
				}
			}
		}
		println("Exported " + n + " entries to " + args[0]);
	}
}
