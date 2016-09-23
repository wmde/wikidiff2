#ifndef WIKIDIFF2_H
#define WIKIDIFF2_H

/** Set WD2_USE_STD_ALLOCATOR depending on whether we're compiling as a PHP module or not */
#if defined(HAVE_CONFIG_H)
	#define WD2_ALLOCATOR PhpAllocator
	#include "php_cpp_allocator.h"
#else
	#define WD2_ALLOCATOR std::allocator
#endif

#include "DiffEngine.h"
#include "Word.h"
#include <string>
#include <vector>
#include <set>


class Wikidiff2 {
	public:
		typedef std::basic_string<char, std::char_traits<char>, WD2_ALLOCATOR<char> > String;
		typedef std::vector<String, WD2_ALLOCATOR<String> > StringVector;
		typedef std::vector<Word, WD2_ALLOCATOR<Word> > WordVector;
		typedef std::vector<int, WD2_ALLOCATOR<int> > IntVector;
		typedef std::set<int, std::less<int>, WD2_ALLOCATOR<int> > IntSet;

		typedef Diff<String> StringDiff;
		typedef Diff<Word> WordDiff;

		const String & execute(const String & text1, const String & text2, int numContextLines);

		inline const String & getResult() const;

	protected:
		enum { MAX_WORD_LEVEL_DIFF_COMPLEXITY = 40000000 };
		String result;

        struct DiffMapEntry
        {
            WordDiff diff;
            int opCharCount[4]= { 0 };
            double similarity;
            int opIndexFrom, opLineFrom, opIndexTo, opLineTo;

            DiffMapEntry(WordVector& words1, WordVector& words2, int opIndexFrom_, int opLineFrom_, int opIndexTo_, int opLineTo_);
        };
        // PhpAllocator can't be specialized for std::pair, so we're using the standard allocator.
        typedef std::map<uint64_t, std::shared_ptr<struct Wikidiff2::DiffMapEntry> > DiffMap;
        DiffMap diffMap;

		virtual void diffLines(const StringVector & lines1, const StringVector & lines2,
				int numContextLines);
		virtual void printAdd(const String & line) = 0;
		virtual void printDelete(const String & line) = 0;
		virtual void printWordDiff(const String & text1, const String & text2, const String & diffID, bool printLeft= true, bool printRight= true) = 0;
		virtual void printBlockHeader(int leftLine, int rightLine) = 0;
		virtual void printContext(const String & input) = 0;

		void printText(const String & input);
		inline bool isLetter(int ch);
		inline bool isSpace(int ch);
		void debugPrintWordDiff(WordDiff & worddiff);

		int nextUtf8Char(String::const_iterator & p, String::const_iterator & charStart,
				String::const_iterator end);

		void explodeWords(const String & text, WordVector &tokens);
		void explodeLines(const String & text, StringVector &lines);
        
        bool printMovedLineDiff(StringDiff & linediff, int opIndex, int opLine);
        
        String makeDiffID(int opIndexFrom, int opLineFrom, int opIndexTo, int opLineTo)
        {
            char ch[128];
            snprintf(ch, sizeof(ch), "diff_%d.%d-%d.%d", opIndexFrom, opLineFrom, opIndexTo, opLineTo);
            return String(ch);
        }
};

inline bool Wikidiff2::isLetter(int ch)
{
	// Standard alphanumeric
	if ((ch >= '0' && ch <= '9') ||
	   (ch == '_') ||
	   (ch >= 'A' && ch <= 'Z') ||
	   (ch >= 'a' && ch <= 'z'))
	{
		return true;
	}
	// Punctuation and control characters
	if (ch < 0xc0) return false;
	// Chinese, Japanese: split up character by character
	if (ch >= 0x3000 && ch <= 0x9fff) return false;
	if (ch >= 0x20000 && ch <= 0x2a000) return false;
	// Otherwise assume it's from a language that uses spaces
	return true;
}

inline bool Wikidiff2::isSpace(int ch)
{
	return ch == ' ' || ch == '\t';
}

inline const Wikidiff2::String & Wikidiff2::getResult() const
{
	return result;
}

inline Wikidiff2::DiffMapEntry::DiffMapEntry(Wikidiff2::WordVector& words1, Wikidiff2::WordVector& words2, int opIndexFrom_, int opLineFrom_, int opIndexTo_, int opLineTo_):
    diff(words1, words2, MAX_WORD_LEVEL_DIFF_COMPLEXITY),
    opIndexFrom(opIndexFrom_), opLineFrom(opLineFrom_), opIndexTo(opIndexTo_), opLineTo(opLineTo_)
{
    int charsTotal= 0;
    for(int i= 0; i<diff.size(); ++i)
    {
        int op= diff[i].op;
        int charCount;
        switch(diff[i].op)
        {
            case DiffOp<Word>::del:
            case DiffOp<Word>::copy:
                charCount= diff[i].from.size();
                break;
            case DiffOp<Word>::add:
                charCount= diff[i].to.size();
                break;
            case DiffOp<Word>::change:
                charCount= std::max( diff[i].from.size(), diff[i].to.size() );
                break;
        }
        opCharCount[op]+= charCount;
        charsTotal+= charCount;
    }
    if(opCharCount[DiffOp<Word>::copy]==0)
        similarity= 0.;
    else
    {
        if(charsTotal)
            similarity= double(opCharCount[DiffOp<Word>::copy]) / charsTotal;
        else
            similarity= 0.0;
    }
}


#endif
