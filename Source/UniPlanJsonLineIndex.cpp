#include "UniPlanJsonLineIndex.h"

#include <cstddef>
#include <string>
#include <vector>

namespace UniPlan
{

namespace
{

// Represent one level of the current JSON path stack. A level is either an
// object key (string) or an array index (int). Path strings are assembled
// via AppendSegment below.
struct FStackFrame
{
    bool mbIsArray = false;
    int mArrayIndex = 0; // current 0-based index within array (live counter)
    std::string mPendingKey; // key last seen inside an object, if any
};

// Append the current path stack as a single canonical path string.
// Example: ["phases", 2, "lanes", 0, "scope"] -> "phases[2].lanes[0].scope"
std::string BuildPath(const std::vector<std::string> &InPathParts)
{
    std::string Out;
    for (const std::string &Part : InPathParts)
    {
        if (Part.empty())
            continue;
        if (Part[0] == '[')
        {
            Out += Part;
        }
        else
        {
            if (!Out.empty())
                Out += ".";
            Out += Part;
        }
    }
    return Out;
}

} // namespace

void FJsonLineIndex::Build(const std::string &InText)
{
    mPathToLine.clear();

    const size_t N = InText.size();
    size_t I = 0;
    int Line = 1;

    // Two collaborating stacks:
    //   FrameStack — one frame per open container (object/array).
    //   PathParts  — canonical path segments appended at every open.
    std::vector<FStackFrame> FrameStack;
    std::vector<std::string> PathParts;

    // State machine: true when the next non-whitespace token in an object
    // should be interpreted as a key.
    bool bExpectKey = false;
    // True when we've just consumed a key and are waiting for ':' then value.
    bool bHaveKey = false;
    std::string CurrentKey;

    while (I < N)
    {
        const char C = InText[I];

        // Track lines.
        if (C == '\n')
        {
            ++Line;
            ++I;
            continue;
        }
        if (C == ' ' || C == '\t' || C == '\r')
        {
            ++I;
            continue;
        }

        // String — either a key or a value.
        if (C == '"')
        {
            const int StringStartLine = Line;
            std::string Str;
            ++I;
            while (I < N)
            {
                const char Ch = InText[I];
                if (Ch == '\\' && I + 1 < N)
                {
                    Str += InText[I + 1];
                    I += 2;
                    continue;
                }
                if (Ch == '"')
                {
                    ++I;
                    break;
                }
                if (Ch == '\n')
                    ++Line;
                Str += Ch;
                ++I;
            }

            if (bExpectKey)
            {
                CurrentKey = std::move(Str);
                bHaveKey = true;
                bExpectKey = false;
                // Record the line where this key declares its value.
                PathParts.push_back(CurrentKey);
                mPathToLine[BuildPath(PathParts)] = StringStartLine;
                PathParts.pop_back();
            }
            else
            {
                // Value string inside array or after colon.
                if (bHaveKey)
                {
                    bHaveKey = false;
                    // Scalar value consumed; path already recorded.
                }
                else if (!FrameStack.empty() && FrameStack.back().mbIsArray)
                {
                    // Array-of-strings element — record index.
                    FStackFrame &Top = FrameStack.back();
                    PathParts.push_back("[" + std::to_string(Top.mArrayIndex) +
                                        "]");
                    mPathToLine[BuildPath(PathParts)] = StringStartLine;
                    PathParts.pop_back();
                }
            }
            continue;
        }

        // Open object.
        if (C == '{')
        {
            FStackFrame Frame;
            Frame.mbIsArray = false;
            if (bHaveKey)
            {
                PathParts.push_back(CurrentKey);
                bHaveKey = false;
            }
            else if (!FrameStack.empty() && FrameStack.back().mbIsArray)
            {
                FStackFrame &Parent = FrameStack.back();
                PathParts.push_back("[" + std::to_string(Parent.mArrayIndex) +
                                    "]");
                // Record this array-element object's start line.
                mPathToLine[BuildPath(PathParts)] = Line;
            }
            FrameStack.push_back(Frame);
            bExpectKey = true;
            ++I;
            continue;
        }

        // Open array.
        if (C == '[')
        {
            FStackFrame Frame;
            Frame.mbIsArray = true;
            Frame.mArrayIndex = 0;
            if (bHaveKey)
            {
                PathParts.push_back(CurrentKey);
                bHaveKey = false;
            }
            FrameStack.push_back(Frame);
            bExpectKey = false;
            ++I;
            continue;
        }

        // Close object.
        if (C == '}')
        {
            if (!PathParts.empty())
            {
                // Pop either the array-index segment or key segment for
                // this object. We pushed one segment per object-open.
                if (!FrameStack.empty() && !FrameStack.back().mbIsArray)
                {
                    if (!PathParts.empty())
                        PathParts.pop_back();
                }
            }
            if (!FrameStack.empty())
                FrameStack.pop_back();
            bExpectKey = false;
            bHaveKey = false;
            ++I;
            continue;
        }

        // Close array.
        if (C == ']')
        {
            if (!PathParts.empty() && !FrameStack.empty() &&
                FrameStack.back().mbIsArray)
            {
                PathParts.pop_back();
            }
            if (!FrameStack.empty())
                FrameStack.pop_back();
            bExpectKey = false;
            bHaveKey = false;
            ++I;
            continue;
        }

        // Comma: advance array index OR re-expect key in object.
        if (C == ',')
        {
            if (!FrameStack.empty())
            {
                if (FrameStack.back().mbIsArray)
                    FrameStack.back().mArrayIndex += 1;
                else
                    bExpectKey = true;
            }
            bHaveKey = false;
            ++I;
            continue;
        }

        // Colon — separator between key and value; ignored.
        if (C == ':')
        {
            ++I;
            continue;
        }

        // Scalar value (number / true / false / null). Skip until delimiter.
        // The path was already recorded when the key was consumed above.
        while (I < N)
        {
            const char Ch = InText[I];
            if (Ch == ',' || Ch == '}' || Ch == ']' || Ch == '\n' ||
                Ch == ' ' || Ch == '\t' || Ch == '\r')
                break;
            ++I;
        }
        bHaveKey = false;
    }
}

int FJsonLineIndex::LineFor(const std::string &InPath) const
{
    auto It = mPathToLine.find(InPath);
    if (It == mPathToLine.end())
        return -1;
    return It->second;
}

} // namespace UniPlan
