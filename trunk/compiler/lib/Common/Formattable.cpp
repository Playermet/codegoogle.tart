/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include "tart/Common/Formattable.h"

#include "llvm/ADT/Twine.h"

//#include <ostream>
#include <iostream>
//#include <sstream>

namespace tart {

// -------------------------------------------------------------------
// Formattable

void Formattable::dump() const {
  FormatStream stream(std::cerr);
  format(stream);
}

const char * Formattable::str() const {
  static std::string temp;
  std::stringstream ss;
  FormatStream stream(ss);
  stream.setFormatOptions(Format_Verbose);
  format(stream);
  stream.flush();
  temp = ss.str();
  return temp.c_str();
}

// -------------------------------------------------------------------
// FormatStream

FormatStream & FormatStream::operator<<(const char * str) {
  llvm::raw_os_ostream::operator<<(str);
  return *this;
}

FormatStream & FormatStream::operator<<(const std::string & str) {
  llvm::raw_os_ostream::operator<<(str);
  return *this;
}

FormatStream & FormatStream::operator<<(int value) {
  llvm::raw_os_ostream::operator<<(value);
  return *this;
}

FormatStream & FormatStream::operator<<(llvm::StringRef str) {
  llvm::raw_os_ostream::operator<<(str);
  return *this;
}

FormatStream & FormatStream::operator<<(const llvm::Twine & str) {
  str.print(*this);
  return *this;
}

FormatStream & FormatStream::operator<<(FormatOptions f) {
  formatOptions_ |= f;
  return *this;
}

}
