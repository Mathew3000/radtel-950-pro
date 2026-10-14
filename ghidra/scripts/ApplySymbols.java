// Apply names and comments from a CSV written by ExportSymbols.java
// (columns: address,kind,name,comment). Run as a postScript after
// analysis so functions already exist; missing functions are created.
// Script args: <symbols.csv>
//@category RT950
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplySymbols extends GhidraScript {

	static List<String> parseCsvLine(String line) {
		List<String> out = new ArrayList<>();
		StringBuilder cur = new StringBuilder();
		boolean inQ = false;
		for (int i = 0; i < line.length(); i++) {
			char c = line.charAt(i);
			if (inQ) {
				if (c == '"' && i + 1 < line.length() && line.charAt(i + 1) == '"') {
					cur.append('"');
					i++;
				}
				else if (c == '"') {
					inQ = false;
				}
				else {
					cur.append(c);
				}
			}
			else if (c == '"') {
				inQ = true;
			}
			else if (c == ',') {
				out.add(cur.toString());
				cur.setLength(0);
			}
			else {
				cur.append(c);
			}
		}
		out.add(cur.toString());
		return out;
	}

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			throw new IllegalArgumentException("usage: ApplySymbols.java <symbols.csv>");
		}
		int applied = 0, skipped = 0;
		List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
		for (String line : lines.subList(1, lines.size())) {
			if (line.isBlank() || line.startsWith("#")) {
				continue;
			}
			List<String> f = parseCsvLine(line);
			Address a = toAddr(f.get(0));
			String kind = f.get(1);
			String name = f.size() > 2 ? f.get(2) : "";
			String comment = f.size() > 3 ? f.get(3).replace("\\n", "\n") : "";
			if (getMemoryBlock(a) == null) {
				printerr("Not in memory, skipped: " + line);
				skipped++;
				continue;
			}
			switch (kind) {
				case "function": {
					Function fn = getFunctionAt(a);
					if (fn == null) {
						disassemble(a);
						fn = createFunction(a, name);
					}
					if (fn == null) {
						printerr("Could not create function: " + line);
						skipped++;
						continue;
					}
					fn.setName(name, SourceType.USER_DEFINED);
					if (!comment.isEmpty()) {
						fn.setComment(comment);
					}
					break;
				}
				case "label":
					createLabel(a, name, true, SourceType.USER_DEFINED);
					break;
				case "comment_plate":
					setPlateComment(a, comment);
					break;
				case "comment_pre":
					setPreComment(a, comment);
					break;
				case "comment_eol":
					setEOLComment(a, comment);
					break;
				default:
					printerr("Unknown kind, skipped: " + line);
					skipped++;
					continue;
			}
			applied++;
		}
		println("Applied " + applied + " entries, skipped " + skipped);
	}
}
