/**
 * Diff formatter, based on code by Steinar H. Gunderson, converted to work with the
 * Dairiki diff engine by Tim Starling
 *
 * GPL.
 */

#include <stdio.h>
#include <string.h>
#include "Wikidiff2.h"
#include <thai/thailib.h>
#include <thai/thwchar.h>
#include <thai/thbrk.h>


void Wikidiff2::diffLines(const StringVector & lines1, const StringVector & lines2,
		int numContextLines)
{
	// first do line-level diff
	StringDiff linediff(lines1, lines2);

	int from_index = 1, to_index = 1;

	// Should a line number be printed before the next context line?
	// Set to true initially so we get a line number on line 1
	bool showLineNumber = true;

	for (int i = 0; i < linediff.size(); ++i) {
		int n, j, n1, n2;
		// Line 1 changed, show heading with no leading context
		if (linediff[i].op != DiffOp<String>::copy && i == 0) {
			printBlockHeader(1, 1);
		}

		switch (linediff[i].op) {
			case DiffOp<String>::add:
				// inserted lines
				n = linediff[i].to.size();
				for (j=0; j<n; j++) {
                    if(!printMovedLineDiff(linediff, i, j))
                        printAdd(*linediff[i].to[j]);
				}
				to_index += n;
				break;
			case DiffOp<String>::del:
				// deleted lines
				n = linediff[i].from.size();
				for (j=0; j<n; j++) {
                    if(!printMovedLineDiff(linediff, i, j))
                        printDelete(*linediff[i].from[j]);
				}
				from_index += n;
				break;
			case DiffOp<String>::copy:
				// copy/context
				n = linediff[i].from.size();
				for (j=0; j<n; j++) {
					if ((i != 0 && j < numContextLines) /*trailing*/
							|| (i != linediff.size() - 1 && j >= n - numContextLines)) /*leading*/ {
						if (showLineNumber) {
							printBlockHeader(from_index, to_index);
							showLineNumber = false;
						}
						printContext(*linediff[i].from[j]);
					} else {
						showLineNumber = true;
					}
					from_index++;
					to_index++;
				}
				break;
			case DiffOp<String>::change:
				// replace, i.e. we do a word diff between the two sets of lines
				n1 = linediff[i].from.size();
				n2 = linediff[i].to.size();
				n = std::min(n1, n2);
				for (j=0; j<n; j++) {
					printWordDiff(*linediff[i].from[j], *linediff[i].to[j]);
				}
				from_index += n;
				to_index += n;
				if (n1 > n2) {
					for (j=n2; j<n1; j++) {
						printDelete(*linediff[i].from[j]);
					}
				} else {
					for (j=n1; j<n2; j++) {
						printAdd(*linediff[i].to[j]);
					}
				}
				break;
		}
		// Not first line anymore, don't show line number by default
		showLineNumber = false;
	}
}


bool Wikidiff2::printMovedLineDiff(StringDiff & linediff, int opIndex, int opLine)
{
    //    look for corresponding moved line for the opposite case in moved-line-map
    //    if moved line exists:
    //        print diff to the moved line, omitting the left/right side for added/deleted line
    uint64_t key= uint64_t(opIndex) << 32 | opLine;
    auto it= diffMap.find(key);
    if(it!=diffMap.end())
    {
        auto best= it->second;
        
        bool printLeft= linediff[opIndex].op==DiffOp<String>::del? true: false;
        bool printRight= !printLeft;
        // XXXX todo: we already have the diff, don't have to do it again, just have to print it
        printWordDiff(*linediff[best->opIndexFrom].from[best->opLineFrom], *linediff[best->opIndexTo].to[best->opLineTo], 
            printLeft, printRight);

#ifdef DEBUG_MOVED_LINES
        char ch[128];
        snprintf(ch, sizeof(ch), "found in diffmap. copy: %d, del: %d, add: %d, change: %d, similarity: %.4f\n", 
            best->opCharCount[DiffOp<Word>::copy], best->opCharCount[DiffOp<Word>::del], best->opCharCount[DiffOp<Word>::add], best->opCharCount[DiffOp<Word>::change], best->similarity);
        result+= "<tr><td /><td class=\"diff-context\" colspan=3>";
        result+= ch;
        snprintf(ch, sizeof(ch), "from: (%d,%d) to: (%d,%d)\n", 
            best->opIndexFrom, best->opLineFrom, best->opIndexTo, best->opLineTo);
        result+= ch;
        result+= "</td></tr>";
#endif // DEBUG_MOVED_LINES
        
        return true;
    }
    
    //    else:
    //        try to find a corresponding moved line in deleted/added lines
    int otherOp= (linediff[opIndex].op==DiffOp<String>::add? DiffOp<String>::del: DiffOp<String>::add);
    std::shared_ptr<DiffMapEntry> found= nullptr;
	for(int i = 0; i < linediff.size(); ++i)
    {
        if(linediff[i].op==otherOp)
        {
            auto& lines= (linediff[opIndex].op==DiffOp<String>::add? linediff[i].from: linediff[i].to);
            for(int k= 0; k < lines.size(); ++k)
            {
                WordVector words1, words2;
                std::shared_ptr<DiffMapEntry> tmp;
                explodeWords(*lines[k], words1);
                if(otherOp==DiffOp<String>::del)
                {
                    explodeWords(*linediff[opIndex].to[opLine], words2);
                    tmp= std::make_shared<DiffMapEntry>(words1, words2, i, k, opIndex, opLine);
                }
                else
                {
                    explodeWords(*linediff[opIndex].from[opLine], words2);
                    tmp= std::make_shared<DiffMapEntry>(words1, words2, opIndex, opLine, i, k);
                }
                if(!found || tmp->similarity > found->similarity)
                    found= tmp;            }
        }
    }
    
    //        if candidate exists:
    //            add candidate to moved-line-map twice, for add/del case
    //            print diff to the moved line, omitting the left/right side for added/deleted line
    if(found && found->similarity > 0.25)
    {
        diffMap[key]= found;
        uint64_t oppositeKey= uint64_t(found->opIndexTo) << 32 | found->opLineTo;
        diffMap[oppositeKey]= found;

        bool printLeft= linediff[opIndex].op==DiffOp<String>::del? true: false;
        bool printRight= !printLeft;
        // XXXX todo: we already have the diff, don't have to do it again, just have to print it
        printWordDiff(*linediff[found->opIndexFrom].from[found->opLineFrom], *linediff[found->opIndexTo].to[found->opLineTo], 
            printLeft, printRight);

#ifdef DEBUG_MOVED_LINES
        char ch[64];
        snprintf(ch, sizeof(ch), "copy: %d, del: %d, add: %d, change: %d, similarity: %.4f\n", 
            found->opCharCount[DiffOp<Word>::copy], found->opCharCount[DiffOp<Word>::del], found->opCharCount[DiffOp<Word>::add], found->opCharCount[DiffOp<Word>::change], found->similarity);
        result+= "<tr><td /><td class=\"diff-context\" colspan=3>";
        result+= ch;
        snprintf(ch, sizeof(ch), "from: (%d,%d) to: (%d,%d)\n", 
            found->opIndexFrom, found->opLineFrom, found->opIndexTo, found->opLineTo);
        result+= ch;
        result+= "</td></tr>";
#endif // DEBUG_MOVED_LINES

        return true;
    }
    
    return false;
}

void Wikidiff2::debugPrintWordDiff(WordDiff & worddiff)
{
	for (unsigned i = 0; i < worddiff.size(); ++i) {
		DiffOp<Word> & op = worddiff[i];
		switch (op.op) {
			case DiffOp<Word>::copy:
				result += "Copy\n";
				break;
			case DiffOp<Word>::del:
				result += "Delete\n";
				break;
			case DiffOp<Word>::add:
				result += "Add\n";
				break;
			case DiffOp<Word>::change:
				result += "Change\n";
				break;
		}
		result += "From: ";
		bool first = true;
		for (int j=0; j<op.from.size(); j++) {
			if (first) {
				first = false;
			} else {
				result += ", ";
			}
			result += "(";
			result += op.from[j]->whole() + ")";
		}
		result += "\n";
		result += "To: ";
		first = true;
		for (int j=0; j<op.to.size(); j++) {
			if (first) {
				first = false;
			} else {
				result += ", ";
			}
			result += "(";
			result += op.to[j]->whole() + ")";
		}
		result += "\n\n";
	}
}

void Wikidiff2::printText(const String & input)
{
	size_t start = 0;
	size_t end = input.find_first_of("<>&");
	while (end != String::npos) {
		if (end > start) {
			result.append(input, start, end - start);
		}
		switch (input[end]) {
			case '<':
				result.append("&lt;");
				break;
			case '>':
				result.append("&gt;");
				break;
			default /*case '&'*/:
				result.append("&amp;");
		}
		start = end + 1;
		end = input.find_first_of("<>&", start);
	}
	// Append the rest of the string after the last special character
	if (start < input.size()) {
		result.append(input, start, input.size() - start);
	}
}

// Weak UTF-8 decoder
// Will return garbage on invalid input (overshort sequences, overlong sequences, etc.)
int Wikidiff2::nextUtf8Char(String::const_iterator & p, String::const_iterator & charStart,
		String::const_iterator end)
{
	int c = 0;
	unsigned char byte;
	int seqLength = 0;
	charStart = p;
	if (p == end) {
		return 0;
	}
	do {
		byte = (unsigned char)*p;
		if (byte < 0x80) {
			c = byte;
			seqLength = 0;
		} else if (byte >= 0xc0) {
			// Start of UTF-8 character
			// If this is unexpected, due to an overshort sequence, we ignore the invalid
			// sequence and resynchronise here
			if (byte < 0xe0) {
				seqLength = 1;
				c = byte & 0x1f;
			} else if (byte < 0xf0) {
				seqLength = 2;
				c = byte & 0x0f;
			} else {
				seqLength = 3;
				c = byte & 7;
			}
		} else if (seqLength) {
			c <<= 6;
			c |= byte & 0x3f;
			--seqLength;
		} else {
			// Unexpected continuation, ignore
		}
		++p;
	} while (seqLength && p != end);
	return c;
}

// Split a string into words
//
// TODO: I think the best way to do this would be to use ICU BreakIterator
// instead of libthai + DIY. Basically you'd run BreakIterators from several
// different locales (en, th, ja) and merge the results, i.e. if a break occurs
// in any locale at a given position, split the string. I don't know if the
// quality of the Thai dictionary in ICU matches the one in libthai, we would
// have to check this somehow.
void Wikidiff2::explodeWords(const String & text, WordVector &words)
{
	// Decode the UTF-8 in the string.
	// * Save the character sizes (in bytes)
	// * Convert the string to TIS-620, which is the internal character set of libthai.
	// * Save the character offsets of any break positions (same format as libthai).

	String tisText, charSizes;
	String::const_iterator suffixEnd, charStart, p;
	IntSet breaks;

	tisText.reserve(text.size());
	charSizes.reserve(text.size());
	wchar_t ch, lastChar;
	thchar_t thaiChar;
	bool hasThaiChars = false;

	p = text.begin();
	ch = nextUtf8Char(p, charStart, text.end());
	lastChar = 0;
	int charIndex = 0;
	while (ch) {
		thaiChar = th_uni2tis(ch);
		if (thaiChar >= 0x80 && thaiChar != THCHAR_ERR) {
			hasThaiChars = true;
		}
		tisText += (char)thaiChar;
		charSizes += (char)(p - charStart);

		if (isLetter(ch)) {
			if (lastChar && !isLetter(lastChar)) {
				breaks.insert(charIndex);
			}
		} else {
			breaks.insert(charIndex);
		}
		charIndex++;
		lastChar = ch;
		ch = nextUtf8Char(p, charStart, text.end());
	}

	// If there were any Thai characters in the string, run th_brk on it and add
	// the resulting break positions
	if (hasThaiChars) {
		IntVector thaiBreakPositions;
		tisText += '\0';
		thaiBreakPositions.resize(tisText.size());
		int numBreaks = th_brk((const thchar_t*)(tisText.data()),
				&thaiBreakPositions[0], thaiBreakPositions.size());
		thaiBreakPositions.resize(numBreaks);
		breaks.insert(thaiBreakPositions.begin(), thaiBreakPositions.end());
	}

	// Add a fake end-of-string character and have a break on it, so that the
	// last word gets added without special handling
	breaks.insert(charSizes.size());
	charSizes += (char)0;

	// Now make the word array by traversing the breaks set
	p = text.begin();
	IntSet::iterator pBrk = breaks.begin();
	String::const_iterator wordStart = text.begin();
	String::const_iterator suffixStart = text.end();

	// If there's a break at the start of the string, skip it
	if (pBrk != breaks.end() && *pBrk == 0) {
		pBrk++;
	}

	for (charIndex = 0; charIndex < charSizes.size(); p += charSizes[charIndex++]) {
		// Assume all spaces are ASCII
		if (isSpace(*p)) {
			suffixStart = p;
		}
		if (pBrk != breaks.end() && charIndex == *pBrk) {
			if (suffixStart == text.end()) {
				words.push_back(Word(wordStart, p, p));
			} else {
				words.push_back(Word(wordStart, suffixStart, p));
			}
			pBrk++;
			suffixStart = text.end();
			wordStart = p;
		}
	}
}

void Wikidiff2::explodeLines(const String & text, StringVector &lines)
{
	String::const_iterator ptr = text.begin();
	while (ptr != text.end()) {
		String::const_iterator ptr2 = std::find(ptr, text.end(), '\n');
		lines.push_back(String(ptr, ptr2));

		ptr = ptr2;
		if (ptr != text.end()) {
			++ptr;
		}
	}
}

const Wikidiff2::String & Wikidiff2::execute(const String & text1, const String & text2, int numContextLines)
{
	// Allocate some result space to avoid excessive copying
	result.clear();
	result.reserve(text1.size() + text2.size() + 10000);

	// Split input strings into lines
	StringVector lines1;
	StringVector lines2;
	explodeLines(text1, lines1);
	explodeLines(text2, lines2);

	// Do the diff
	diffLines(lines1, lines2, numContextLines);

	// Return a reference to the result buffer
	return result;
}
