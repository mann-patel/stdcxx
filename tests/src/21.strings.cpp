/************************************************************************
 *
 * 21.strings.cpp - definitions of helpers used in clause 21 tests
 *
 * $Id$
 *
 ***************************************************************************
 *
 * Copyright 2006 The Apache Software Foundation or its licensors,
 * as applicable.
 *
 * Copyright 2006 Rogue Wave Software.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 **************************************************************************/

// expand _TEST_EXPORT macros
#define _RWSTD_TEST_SRC

#include <21.strings.h>

#include <cmdopt.h>       // for rw_enabled()
#include <driver.h>       // for rw_info()
#include <environ.h>      // for rw_putenv()
#include <rw_char.h>      // for rw_expand()
#include <rw_printf.h>    // for rw_asnprintf()

#include <ctype.h>        // for isdigit()
#include <stdarg.h>       // for va_arg, ...
#include <stddef.h>       // for size_t
#include <stdlib.h>       // for free()
#include <string.h>       // for memset()

/**************************************************************************/

static const char
_rw_this_file[] = __FILE__;


static const char* const
_rw_char_names[] = {
    "char", "wchar_t", "UserChar"
};


static const char* const
_rw_traits_names[] = {
    "char_traits", "UserTraits"
};


static const char* const
_rw_alloc_names[] = {
    "allocator", "UserAlloc"
};

// order of elements depends on the values of StringIds::FuncId
static const char* const
_rw_func_names[] = {
    "append", "assign", "erase", "insert", "replace", "operator+=", "find", 
    "rfind", "find_first_of", "find_last_of", "find_first_not_of", 
    "find_last_not_of", "compare", "substr", "operator[]", "at", "copy",
    0 /* special handling for the ctor */, "operator=", "swap", "push_back",
    "operator+", "operator==", "operator!=", "operator<", "operator<=",
    "operator>", "operator>="
};

/**************************************************************************/

const size_t MAX_OVERLOADS = 32;

// disabled (-1) or explicitly enabled (+1) for each overload
// of the string function being tested
static int
_rw_opt_func [MAX_OVERLOADS];

// array of tests each exercising a single string function
static const StringTest*
_rw_string_tests;

// size of the array above
static size_t
_rw_string_test_count;

static int
_rw_opt_char_types [3];

static int
_rw_opt_traits_types [2];

static int
_rw_opt_alloc_types [2];

static int
_rw_opt_no_exceptions;

static int
_rw_opt_no_exception_safety;

static int
_rw_opt_self_ref;

/**************************************************************************/

static size_t
_rw_get_func_inx (StringIds::OverloadId fid)
{
    size_t inx = _RWSTD_SIZE_MAX;

    for (size_t i = 0; _rw_string_test_count; ++i) {
        if (fid == _rw_string_tests [i].which) {
            inx = i;
            break;
        }
    }

    RW_ASSERT (inx < _RWSTD_SIZE_MAX);

    return inx;
}

/**************************************************************************/

void StringState::
assert_equal (const StringState &state, int line, int case_line,
              const char *when) const
{
    const int equal =
           data_ == state.data_
        && size_ == state.size_
        && capacity_ == state.capacity_;

    rw_assert (equal, 0, case_line,
               "line %d: %{$FUNCALL}: object state unexpectedly changed "
               "from { %#p, %zu, %zu } to { %#p, %zu, %zu } (data, size, "
               "capacity) after %s",
               line, data_, size_, capacity_,
               state.data_, state.size_, state.capacity_,
               when);
}

/**************************************************************************/

static const char*
_rw_class_name (const StringFunc &func)
{
    if (   StringIds::DefaultTraits == func.traits_id_
        && StringIds::DefaultAlloc == func.alloc_id_) {

        if (StringIds::Char == func.char_id_)
            return "string";

        if (StringIds::WChar == func.char_id_)
            return "wstring";
    }

    return "basic_string";
}

/**************************************************************************/

// appends the signature of the function specified by which
// to the provided buffer; when the second argument is null,
// appends the mnemonic representing the signature, including
// the name of the function, as specified by the third argument
static void
_rw_sigcat (char **pbuf, size_t *pbufsize,
            const StringFunc      *func,
            StringIds::OverloadId  which = StringIds::OverloadId ())
{
    // for convenience
    typedef StringIds Ids;

    if (func)
        which = func->which_;

    // determine whether the function is a member function
    const bool is_member = Ids::bit_member & which;

    // get the bitmap describing the function's argument types
    int argmap = (which & ~Ids::bit_member) >> Ids::fid_bits;

    // determine whether the function is a const member function
    bool is_const_member =
        is_member && Ids::arg_cstr == (argmap & Ids::arg_mask);

    // remove the *this argument if the function is a member
    if (is_member)
        argmap >>= Ids::arg_bits;

    const char* funcname = 0;

    if (0 == func) {
        const Ids::FuncId fid = Ids::FuncId (which & StringIds::fid_mask);

        switch (fid) {
            // translate names with funky characters to mnemonics
        case Ids::fid_ctor:          funcname = "ctor"; break;
        case Ids::fid_op_plus_eq:    funcname = "op_plus_eq"; break;
        case Ids::fid_op_index:      funcname = "op_index"; break;
        case Ids::fid_op_set:        funcname = "op_assign"; break;
        case Ids::fid_op_plus:       funcname = "op_plus"; break;
        case Ids::fid_op_equal:      funcname = "op_equal"; break;
        case Ids::fid_op_not_equal:  funcname = "op_not_equal"; break;
        case Ids::fid_op_less:       funcname = "op_less"; break;
        case Ids::fid_op_less_equal: funcname = "op_less_equal"; break;
        case Ids::fid_op_greater:    funcname = "op_greater"; break;
        case Ids::fid_op_greater_equal: funcname = "op_greater_equal"; break;

        case Ids::fid_compare:
        case Ids::fid_copy:
        case Ids::fid_find:
        case Ids::fid_find_first_not_of:
        case Ids::fid_find_first_of:
        case Ids::fid_find_last_not_of:
        case Ids::fid_find_last_of:
        case Ids::fid_rfind:
        case Ids::fid_substr:
            // prevent appending the "_const" bit to the mnemonics
            // of member functions not overloaded on const
            is_const_member = false;

            // fall through

        default: {
            // determine the string function name (for brief output)
            const size_t nfuncs =
                sizeof _rw_func_names / sizeof *_rw_func_names;

            RW_ASSERT (size_t (fid) < nfuncs);

            funcname = _rw_func_names [fid];
            RW_ASSERT (0 != funcname);
            break;
        }
        }
    }

    rw_asnprintf (pbuf, pbufsize, "%{+}%{?}%s%{?}_const%{;}%{:}(%{;}",
                  0 == func, funcname, is_const_member);

    // iterate through the map of argument types one field at a time
    // determining and formatting the type of each argument until
    // void is reached
    for (size_t argno = 0; argmap; ++argno, argmap >>= Ids::arg_bits) {

        const char* pfx = "";
        const char* sfx = "";

        const int argtype = argmap & Ids::arg_mask;

        const char* tname = 0;

        if (func) {
            switch (argtype) {
            case Ids::arg_size:  tname = "size_type"; break;
            case Ids::arg_val:   tname = "value_type"; break;
            case Ids::arg_ptr:   tname = "pointer"; break;
            case Ids::arg_cptr:  tname = "const_pointer"; break;
            case Ids::arg_ref:   tname = "reference"; break;
            case Ids::arg_cref:  tname = "const_reference"; break;
            case Ids::arg_iter:  tname = "iterator"; break;
            case Ids::arg_citer: tname = "const_iterator"; break;
            case Ids::arg_range: tname = "InputIterator, InputIterator"; break;
            case Ids::arg_alloc: tname = "const allocator_type&"; break;
            case Ids::arg_cstr:
                pfx   = "const ";
                // fall through
            case Ids::arg_str:
                tname = _rw_class_name (*func);
                sfx   = "&";
                break;
            }
        }
        else {
            switch (argtype) {
            case Ids::arg_size:  tname = "size"; break;
            case Ids::arg_val:   tname = "val"; break;
            case Ids::arg_ptr:   tname = "ptr"; break;
            case Ids::arg_cptr:  tname = "cptr"; break;
            case Ids::arg_ref:   tname = "ref"; break;
            case Ids::arg_cref:  tname = "cref"; break;
            case Ids::arg_iter:  tname = "iter"; break;
            case Ids::arg_citer: tname = "citer"; break;
            case Ids::arg_range: tname = "range"; break;
            case Ids::arg_alloc: tname = "alloc"; break;
            case Ids::arg_str:   tname = "str"; break;
            case Ids::arg_cstr:  tname = "cstr"; break;
            }
        }

        RW_ASSERT (0 != tname);

        if (   0 == func || is_member
            || Ids::arg_str != argtype && Ids::arg_cstr != argtype) {
            // append the name or mnemonic of the argument type
            rw_asnprintf (pbuf, pbufsize, "%{+}%{?}_%{:}%{?}, %{;}%{;}%s%s%s",
                          0 == func, 0 < argno, pfx, tname, sfx);
        }
        else {
            // in non-member functions use ${CLASS} to format
            // the basic_string argument in order to expand
            // its template argument list
            rw_asnprintf (pbuf, pbufsize,
                          "%{+}%{?}, %{;}%{?}const %{$CLASS}&%{;}",
                          0 < argno, Ids::arg_cstr == argtype);
            
        }
    }

    if (func)
        rw_asnprintf (pbuf, pbufsize, "%{+})%{?} const%{;}", is_const_member);
}

/**************************************************************************/

static bool
_rw_uses_alloc (StringIds::OverloadId which)
{
    // get the bitmap describing the function's argument types
    int argmap = (which & ~StringIds::bit_member) >> StringIds::fid_bits;

    for (; argmap; argmap >>= StringIds::arg_bits) {
        if ((argmap & StringIds::arg_alloc) == StringIds::arg_alloc)
            return true;
    }

    return false;
}

/**************************************************************************/

// sets the {CLASS}, {FUNC}, {FUNCSIG}, and optionally {FUNCALL}
// environment variables as follows:
// CLASS:   the name of basic_string specialization
// FUNC:    the name of the basic_string function
// FUNCSIG: the name and signature of a specific overload
//          of the basic_string function
// FUNCALL: a string describing the call to the basic_string function
//          with function with function arguments expanded (as specified
//          by the TestCase argument)
static void
_rw_setvars (const StringFunc     &func,
             const StringTestCase *pcase = 0)
{
    char*  buf     = 0;
    size_t bufsize = 0;

    const char* const class_name = _rw_class_name (func);

    if (0 == pcase) {
        // set the {charT}, {Traits}, and {Allocator} environment
        // variables to the name of the character type and the
        // Traits and Allocator specializations
        rw_putenv ("charT=");
        rw_fprintf (0, "%{charT:=*}", _rw_char_names [func.char_id_]);

        rw_putenv ("Traits=");
        rw_fprintf (0, "%{Traits:=*}", _rw_traits_names [func.traits_id_]);

        rw_putenv ("Allocator=");
        rw_fprintf (0, "%{Allocator:=*}", _rw_alloc_names [func.alloc_id_]);

        // set the {CLASS}, {FUNC}, and {FUNCSIG} environment variables
        // to the name of the specialization of the template, the name
        // of the string function, and the name of the overload of the
        // string function, respectively, when no test case is given

        if (   StringIds::DefaultTraits == func.traits_id_
            && (   StringIds::Char == func.char_id_
                || StringIds::WChar == func.char_id_)) {
            // format std::string and std::wstring
            rw_asnprintf (&buf, &bufsize, "std::%{?}w%{;}string",
                          StringIds::WChar == func.char_id_);
        }
        else {
            // format std::basic_string specializations other than
            // std::string and std::wstring, leaving out the name
            // of the default allocator for brevity
            rw_asnprintf (&buf, &bufsize,
                          "std::basic_string<%s, %s<%1$s>%{?}, %s<%1$s>%{;}>",
                          _rw_char_names [func.char_id_],
                          _rw_traits_names [func.traits_id_],
                          StringIds::DefaultAlloc != func.alloc_id_,
                          _rw_alloc_names [func.alloc_id_]);
        }

        // set the {CLASS} variable to the name of the specialization
        // of basic_string
        rw_putenv ("CLASS=");
        rw_fprintf (0, "%{$CLASS:=*}", buf);

        // determine the string function name
        const size_t funcinx = func.which_ & StringIds::fid_mask;
        const size_t nfuncs =  sizeof _rw_func_names / sizeof *_rw_func_names;

        RW_ASSERT (funcinx < nfuncs);

        free (buf);
        buf     = 0;
        bufsize = 0;

        // get the undecorated function name; ctors are treated
        // specially so that we can have string, wstring, or
        // basic_string, depending on the template arguments
        const char* const funcname = _rw_func_names [funcinx] ?
            _rw_func_names [funcinx] : class_name;

        // determine whether the function is a member function
        const bool is_member = func.which_ & StringIds::bit_member;

        // set the {FUNC} variable to the unqualified/undecorated
        // name of the string function (member or otherwise)
        rw_asnprintf (&buf, &bufsize, "%{?}std::%{;}%s",
                      !is_member, funcname);

        rw_putenv ("FUNC=");
        rw_fprintf (0, "%{$FUNC:=*}", buf);

        // append the function signature
        _rw_sigcat (&buf, &bufsize, &func);

        rw_putenv ("FUNCSIG=");
        rw_fprintf (0, "%{$FUNCSIG:=*}", buf);
        free (buf);

        return;
    }

    // do the function call arguments reference *this?
    const bool self = 0 == pcase->arg;

    char str_buf [256];
    char arg_buf [256];
    
    char *str;
    char *arg;

    size_t str_len = sizeof str_buf;
    size_t arg_len = sizeof arg_buf;

    if (pcase->str)
        str = rw_expand (str_buf, pcase->str, pcase->str_len, &str_len);
    else
        str = 0;

    if (pcase->arg)
        arg = rw_expand (arg_buf, pcase->arg, pcase->arg_len, &arg_len);
    else
        arg = 0;

    // determine whether the function is a member function
    const bool is_member = func.which_ & StringIds::bit_member;

    // determine whether the function is a ctor
    bool is_ctor = StringIds::fid_ctor == (func.which_ & StringIds::fid_mask);

    if (is_ctor) {
        // for ctors append just the class name here
        // the class name will inserted below during argument
        // formatting
        rw_asnprintf (&buf, &bufsize, "%{$CLASS}::%s", class_name);
    }
    else if (is_member) {
        // for other members append the ctor argument(s) followed
        // by the string member function name
        rw_asnprintf (&buf, &bufsize,
                      "%{$CLASS} (%{?}%{#*s}%{;}).%{$FUNC} ",
                      str != 0, int (str_len), str);
    }
    else {
        // for non-members append just the function name here
        // the class name will inserted below during argument
        // formatting
        rw_asnprintf (&buf, &bufsize, "%{$FUNC} ");
    }

    // compute the end offsets for convenience
    const size_t range1_end = pcase->off + pcase->size;
    const size_t range2_end = pcase->off2 + pcase->size2;

    const bool use_alloc = _rw_uses_alloc (func.which_);

    // format and append string function arguments abbreviating complex
    // expressions as much as possible to make them easy to understand
    switch (func.which_) {
    case StringIds::append_cptr:
    case StringIds::assign_cptr:
    case StringIds::op_plus_eq_cptr:
    case StringIds::find_cptr:
    case StringIds::rfind_cptr:
    case StringIds::find_first_of_cptr:
    case StringIds::find_last_of_cptr:
    case StringIds::find_first_not_of_cptr:
    case StringIds::find_last_not_of_cptr:
    case StringIds::compare_cptr:
    case StringIds::op_set_cptr:
    case StringIds::ctor_cptr:
    case StringIds::ctor_cptr_alloc:
        // format self-referential ptr argument without size as c_str()
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{?}c_str()%{:}%{#*s}%{;}"
                      "%{?}, const allocator_type&%{;})",
                      self, int (arg_len), arg, use_alloc);
        break;

    case StringIds::append_cstr:
    case StringIds::assign_cstr:
    case StringIds::op_plus_eq_cstr:
    case StringIds::find_cstr:
    case StringIds::rfind_cstr:
    case StringIds::find_first_of_cstr:
    case StringIds::find_last_of_cstr:
    case StringIds::find_first_not_of_cstr:
    case StringIds::find_last_not_of_cstr:
    case StringIds::compare_cstr:
    case StringIds::ctor_cstr:
    case StringIds::ctor_cstr_alloc:
    case StringIds::op_set_cstr:
    case StringIds::swap_str:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{?}*this%{:}%s(%{#*s})%{;}"
                      "%{?}, const allocator_type&%{;})",
                      self, class_name, int (arg_len), arg, use_alloc);
        break;

    case StringIds::append_cptr_size:
    case StringIds::assign_cptr_size:
    case StringIds::copy_ptr_size:
    case StringIds::ctor_cptr_size:
    case StringIds::ctor_cptr_size_alloc:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}data()%{:}%{#*s}%{;}, %zu"
                      "%{?}, const allocator_type&%{;})",
                      self, int (arg_len), arg, pcase->size, use_alloc);
        break;

    case StringIds::find_cptr_size:
    case StringIds::rfind_cptr_size:
    case StringIds::find_first_of_cptr_size:
    case StringIds::find_last_of_cptr_size:
    case StringIds::find_first_not_of_cptr_size:
    case StringIds::find_last_not_of_cptr_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}data()%{:}%{#*s}%{;}, %zu)",
                      self, int (arg_len), arg, pcase->off);
        break;

    case StringIds::find_cstr_size:
    case StringIds::rfind_cstr_size:
    case StringIds::find_first_of_cstr_size:
    case StringIds::find_last_of_cstr_size:
    case StringIds::find_first_not_of_cstr_size:
    case StringIds::find_last_not_of_cstr_size:
    case StringIds::ctor_cstr_size:
    case StringIds::ctor_cstr_size_alloc:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}*this%{:}%s(%{#*s})%{;}, %zu"
                      "%{?}, const allocator_type%{;})",
                      self, class_name, int (arg_len), arg, pcase->off,
                      use_alloc);
        break;

    case StringIds::find_cptr_size_size:
    case StringIds::rfind_cptr_size_size:
    case StringIds::find_first_of_cptr_size_size:
    case StringIds::find_last_of_cptr_size_size:
    case StringIds::find_first_not_of_cptr_size_size:
    case StringIds::find_last_not_of_cptr_size_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}data()%{:}%{#*s}%{;}, %zu, %zu)",
                      self, int (arg_len), arg,
                      pcase->off, pcase->size);
        break;

    case StringIds::copy_ptr_size_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}data()%{:}%{#*s}%{;}, %zu, %zu)",
                      self, int (arg_len), arg,
                      pcase->size, pcase->off);
        break;

    case StringIds::append_cstr_size_size:
    case StringIds::assign_cstr_size_size:
    case StringIds::ctor_cstr_size_size:
    case StringIds::ctor_cstr_size_size_alloc:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}*this%{:}%s(%{#*s})%{;}, %zu, %zu"
                      "%{?}, const allocator_type%{;})",
                      self, class_name, int (arg_len), arg,
                      pcase->off, pcase->size, use_alloc);
        break;

    case StringIds::append_size_val:
    case StringIds::assign_size_val:
    case StringIds::ctor_size_val:
    case StringIds::ctor_size_val_alloc:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%zu, %{#c}%{?}, const allocator_type&%{;})",
                      pcase->size, pcase->val, use_alloc);
        break;

    case StringIds::append_range:
    case StringIds::assign_range:
    case StringIds::ctor_range:
    case StringIds::ctor_range_alloc:
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%{?}begin()%{:}Iterator(%{#*s})%{;}"
                      "%{?} + %zu%{;}, "
                      "%{?}begin()%{:}Iterator(...)%{;}"
                      "%{?} + %zu%{;}"
                      "%{?}, const allocator_type&%{;})",
                      self, int (arg_len), arg,
                      0 != pcase->off, pcase->off,
                      self, 0 != range1_end, range1_end, use_alloc);
        break;

    case StringIds::insert_size_cptr:
        // format self-referential ptr argument without size as c_str()
        rw_asnprintf (&buf, &bufsize, 
                      "%{+}(%zu, %{?}c_str()%{:}%{#*s}%{;})",
                      pcase->off, self, int (arg_len), arg);
        break;

    case StringIds::insert_size_cstr:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize,  
                      "%{+}(%zu, %{?}*this%{:}%s(%{#*s})%{;})",
                      pcase->off, self, class_name, int (arg_len), arg);
        break;

    case StringIds::insert_size_cptr_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%zu, %{?}data()%{:}%{#*s}%{;}, %zu)", 
                      pcase->off, self, int (arg_len), arg,
                      pcase->size2);
        break;

    case StringIds::insert_size_cstr_size_size:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%zu, %{?}*this%{:}%s(%{#*s})%{;}, %zu, %zu)",
                      pcase->off, self, class_name, int (arg_len), arg,
                      pcase->off2, pcase->size2);
        break;

    case StringIds::insert_size_size_val:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%zu, %zu, %{#c})",
                      pcase->off, pcase->size2, pcase->val);
        break;

    case StringIds::insert_iter_val:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(begin()%{?} + %zu%{;}, %{#c})",
                      0 != pcase->off, pcase->off, pcase->val);
        break;

    case StringIds::insert_iter_size_val:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(begin()%{?} + %zu%{;}, %zu, %{#c})",
                      0 != pcase->off, pcase->off, pcase->size, pcase->val);
        break;

    case StringIds::insert_iter_range:
        rw_asnprintf (&buf, &bufsize, "%{+}(begin()%{?} + %zu%{;}, "
                      "%{?}begin()%{:}Iterator(%{#*s})%{;}"
                      "%{?} + %zu%{;}, "
                      "%{?}begin()%{:}Iterator(...)%{?} + %zu%{;})",
                      0 != pcase->off, pcase->off,
                      self, int (arg_len), arg,
                      0 != pcase->off2, pcase->off2,
                      self, 0 != range2_end, range2_end);
        break;

    case StringIds::replace_size_size_cptr:
    case StringIds::compare_size_size_cptr:
        // format self-referential ptr argument without size as c_str()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%zu, %zu, %{?}c_str()%{:}%{#*s}%{;})",
                      pcase->off, pcase->size, self,
                      int (arg_len), arg);
        break;

    case StringIds::replace_size_size_cstr:
    case StringIds::compare_size_size_cstr:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%zu, %zu, %{?}*this%{:}%s(%{#*s})%{;})",
                      pcase->off, pcase->size, self, class_name,
                      int (arg_len), arg);
        break;

    case StringIds::replace_size_size_cptr_size:
    case StringIds::compare_size_size_cptr_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "%zu, %zu, %{?}data()%{:}%{#*s}%{;}, %zu)", 
                      pcase->off, pcase->size, self,
                      int (arg_len), arg, pcase->size2);
        break;

    case StringIds::replace_size_size_cstr_size_size:
    case StringIds::compare_size_size_cstr_size_size:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}(%zu, %zu, "
                      "%{?}*this%{:}%s(%{#*s})%{;}, %zu, %zu)",
                      pcase->off, pcase->size,
                      self, class_name, int (arg_len), arg,
                      pcase->off2, pcase->size2);
        break;

    case StringIds::replace_size_size_size_val:
        rw_asnprintf (&buf, &bufsize, 
                      "%{+}(%zu, %zu, %zu, %{#c})",
                      pcase->off, pcase->size, pcase->size2, pcase->val);
        break;

    case StringIds::replace_iter_iter_cptr:
        // format self-referential ptr argument without size as c_str()
        rw_asnprintf (&buf, &bufsize, "%{+}(begin()%{?} + %zu%{;}, "
                      "begin()%{?} + %zu%{;}, "
                      "%{?}c_str()%{:}%{#*s}%{;})",
                      0 != pcase->off, pcase->off,
                      0 != range1_end, range1_end,
                      self, int (arg_len), arg);
        break;

    case StringIds::replace_iter_iter_cstr:
        // format self-referential str argument as *this
        rw_asnprintf (&buf, &bufsize, "%{+}(begin()%{?} + %zu%{;}, "
                      "begin()%{?} + %zu%{;}, "
                      "%{?}*this%{:}%s(%{#*s})%{;})",
                      0 != pcase->off, pcase->off,
                      0 != range1_end, range1_end,
                      self, class_name, int (arg_len), arg);
        break;

    case StringIds::replace_iter_iter_cptr_size:
        // format self-referential ptr argument with size as data()
        rw_asnprintf (&buf, &bufsize, "%{+}(begin()%{?} + %zu%{;}, "
                      "begin()%{?} + %zu%{;}, " 
                      "%{?}data()%{:}%{#*s}%{;}, %zu)", 
                      0 != pcase->off, pcase->off,
                      0 != range1_end, range1_end,
                      self, int (arg_len), arg, pcase->size2);
        break;

    case StringIds::replace_iter_iter_size_val:
        rw_asnprintf (&buf, &bufsize, 
                      "%{+}(begin()%{?} + %zu%{;}, begin()%{? + %zu%{;}, "
                      "%zu, %{#c})",
                      0 != pcase->off, pcase->off, 0 != range1_end, range1_end,
                      pcase->size2, pcase->val);
        break;

    case StringIds::replace_iter_iter_range:
        rw_asnprintf (&buf, &bufsize, "%{+}("
                      "begin()%{?} + %zu%{;}, "
                      "begin()%{?} + %zu%{;}, "
                      "%{?}begin()%{:}Iterator(%{#*s})%{;}"
                      "%{?} + %zu%{;}, "
                      "%{?}begin()%{:}Iterator(...)%{;}%{?} + %zu%{;})",
                      0 != pcase->off, pcase->off,
                      0 != range1_end, range1_end,
                      self, int (arg_len), arg,
                      0 != pcase->off2, pcase->off2,
                      self, 0 != range2_end, range2_end);
        break;

    case StringIds::op_plus_eq_val:
    case StringIds::find_val:
    case StringIds::rfind_val:
    case StringIds::find_first_of_val:
    case StringIds::find_last_of_val:
    case StringIds::find_first_not_of_val:
    case StringIds::find_last_not_of_val:
    case StringIds::op_set_val:
    case StringIds::push_back_val:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{#c})", pcase->val);
        break;

    case StringIds::find_val_size:
    case StringIds::rfind_val_size:
    case StringIds::find_first_of_val_size:
    case StringIds::find_last_of_val_size:
    case StringIds::find_first_not_of_val_size:
    case StringIds::find_last_not_of_val_size:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{#c}, %zu)", pcase->val, pcase->off);
        break;

    case StringIds::erase_void:
    case StringIds::substr_void:
    case StringIds::ctor_void:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}()");
        break;

    case StringIds::ctor_alloc:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(const allocator_type&)");
        break;
        
    case StringIds::erase_size:
    case StringIds::substr_size:
    case StringIds::op_index_size:
    case StringIds::at_size:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%zu)", pcase->off);
        break;

    case StringIds::op_index_const_size:
    case StringIds::at_const_size:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%zu) const", pcase->off);
        break;

    case StringIds::erase_size_size:
    case StringIds::substr_size_size:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%zu, %zu)", pcase->off, pcase->size);
        break;

    case StringIds::erase_iter:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(begin()%{?} + %zu%{;})",
                      0 != pcase->off, pcase->off);
        break;

    case StringIds::erase_iter_iter:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(begin()%{?} + %zu%{;}, begin()%{?} + %zu%{;})", 
                      0 != pcase->off, pcase->off,
                      0 != range1_end, range1_end);
        break;

    case StringIds::op_plus_cptr_cstr:
    case StringIds::op_equal_cptr_cstr:
    case StringIds::op_not_equal_cptr_cstr:
    case StringIds::op_less_cptr_cstr:
    case StringIds::op_less_equal_cptr_cstr:
    case StringIds::op_greater_cptr_cstr:
    case StringIds::op_greater_equal_cptr_cstr:
        // format zero ptr argument without size as arg.c_str()
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{?}arg2.c_str()%{:}%{#*s}%{;}, "
                      "%{$CLASS}(%{#*s}))",
                      0 == str, int (str_len), str, int (arg_len), arg);
        break;

    case StringIds::op_plus_cstr_cptr:
    case StringIds::op_equal_cstr_cptr:
    case StringIds::op_not_equal_cstr_cptr:
    case StringIds::op_less_cstr_cptr:
    case StringIds::op_less_equal_cstr_cptr:
    case StringIds::op_greater_cstr_cptr:
    case StringIds::op_greater_equal_cstr_cptr:
        // format zero ptr argument without size as arg.c_str()
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{$CLASS}(%{#*s}), "
                      "%{?}arg1.c_str()%{:}%{#*s}%{;})",
                      int (str_len), str, self, int (arg_len), arg);
        break;

    case StringIds::op_plus_cstr_cstr:
    case StringIds::op_equal_cstr_cstr:
    case StringIds::op_not_equal_cstr_cstr:
    case StringIds::op_less_cstr_cstr:
    case StringIds::op_less_equal_cstr_cstr:
    case StringIds::op_greater_cstr_cstr:
    case StringIds::op_greater_equal_cstr_cstr:
        // format zero str argument without size as arg
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{?}arg2%{:}%{$CLASS}(%{#*s})%{;}, "
                      "%{?}arg1%{:}%{$CLASS}(%{#*s})%{;})",
                      0 == str, int (str_len), str, self, int (arg_len), arg);
        break;

    case StringIds::op_plus_cstr_val:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{$CLASS}(%{#*s}), %{#c})",
                      int (arg_len), arg, pcase->val);
        break;

    case StringIds::op_plus_val_cstr:
        rw_asnprintf (&buf, &bufsize,
                      "%{+}(%{#c}, %{$CLASS}(%{#*s}))",
                      pcase->val, int (arg_len), arg);
        break;

    default:
        RW_ASSERT (!"test logic error: unknown overload");
    }

    rw_putenv ("FUNCALL=");
    rw_fprintf (0, "%{$FUNCALL:=*}", buf);
    free (buf);

    if (str != str_buf)
        delete[] str;

    if (arg != arg_buf)
        delete[] arg;
}

/**************************************************************************/

static void
_rw_test_case (const StringFunc     &func,
               const StringTestCase &tcase,
               StringTestFunc       *test_callback)
{
    // check to see if this is an exception safety test case
    // and avoid running it when exception safety has been
    // disabled via a command line option
    if (-1 == tcase.bthrow && _rw_opt_no_exception_safety) {

        // issue only the first note
        rw_note (1 < _rw_opt_no_exception_safety++, _rw_this_file, __LINE__,
                 "exception safety tests disabled");
        return;
    }

    // check to see if this is a test case that involves the throwing
    // of an exception and avoid running it when exceptions have been
    // disabled
    if (tcase.bthrow && _rw_opt_no_exceptions) {

        // issue only the first note
        rw_note (1 < _rw_opt_no_exceptions++, _rw_this_file, __LINE__,
                 "exception tests disabled");
        return;
    }

    const bool self_ref = 0 == tcase.arg;

    // check for tests exercising self-referential modifications
    // (e.g., insert(1, *this))
    if (_rw_opt_self_ref < 0 && self_ref) {
        // issue only the first note
        rw_note (0, _rw_this_file, tcase.line,
                 "self-referential test disabled");
        return;
    }
    else if (0 < _rw_opt_self_ref && !self_ref) {
        // issue only the first note
        rw_note (0, _rw_this_file, tcase.line,
                 "non-self-referential test disabled");
        return;
    }

    // check to see if the test case is enabled
    if (rw_enabled (tcase.line)) {

        // set the {FUNCALL} environment variable to describe
        // the function call specified by this test case
        _rw_setvars (func, &tcase);

        // invoke the test function
        test_callback (func, tcase);
    }
    else
        rw_note (0, _rw_this_file, tcase.line,
                 "test on line %d disabled", tcase.line);
}

/**************************************************************************/

static void
_rw_toggle_options (int *opts, size_t count)
{
    for (size_t i = 0; i != count; ++i) {
        if (0 < opts [i]) {
            // if one or more options has been explicitly enabled
            // treat all those that haven't been as if they had
            // been disabled
            for (i = 0; i != count; ++i) {
                if (0 == opts [i])
                    opts [i] = -1;
            }
            break;
        }
    }
}

static StringTestFunc*
_rw_test_callback;

static int
_rw_run_test (int, char*[])
{
#ifdef _RWSTD_NO_EXCEPTIONS

    rw_note (0, 0, 0, "exception tests disabled (macro "
             "_RWSTD_NO_EXCEPTIONS #defined)");

    // disable all exception tests and avoid further notes
    _rw_no_exceptions       = 2;
    _rw_no_exception_safety = 2;

#endif   // _RWSTD_NO_EXCEPTIONS

#ifdef _RWSTD_NO_WCHAR_T

    rw_note (0, 0, 0, "wchar_t tests disabled (macro "
             "_RWSTD_NO_WCHAR_T #defined)");

    // disable wchar_t tests and avoid further notes
    _rw_opt_char_types [StringIds::WChar] = -2;

#endif   // _RWSTD_NO_WCHAR_T

    // see if any option controlling a string function has been
    // explicitly enabled and if so disable all those that haven't
    // been (i.e., so that --enable-foo-size will have the effect
    // of specifying --disable-foo-val and --disable-foo-range,
    // given the three overloads of foo)
    const size_t nopts = sizeof _rw_opt_func / sizeof *_rw_opt_func;
    _rw_toggle_options (_rw_opt_func, nopts);

    static const StringIds::CharId char_types[] = {
        StringIds::Char, StringIds::WChar, StringIds::UChar
    };

    static const StringIds::TraitsId traits_types[] = {
        StringIds::DefaultTraits, StringIds::UserTraits
    };

    static const StringIds::AllocId alloc_types[] = {
        StringIds::DefaultAlloc, StringIds::UserAlloc
    };

    const size_t n_char_types   = sizeof char_types / sizeof *char_types;
    const size_t n_traits_types = sizeof traits_types / sizeof *traits_types;
    const size_t n_alloc_types  = sizeof alloc_types / sizeof *alloc_types;

    // see if any option controlling the basic_string template arguments
    // explicitly enabled and if so disable all those that haven't been
    _rw_toggle_options (_rw_opt_char_types, n_char_types);
    _rw_toggle_options (_rw_opt_traits_types, n_traits_types);
    _rw_toggle_options (_rw_opt_alloc_types, n_alloc_types);

    // exercise different charT specializations last
    for (size_t i = 0; i != n_char_types; ++i) {

        if (_rw_opt_char_types [i] < 0) {
            // issue only the first note
            rw_note (-1 > _rw_opt_char_types [i]--,
                     _rw_this_file, __LINE__,
                     "%s tests disabled", _rw_char_names [i]);
            continue;
        }

        // exercise all specializations on Traits before those on charT
        for (size_t j = 0; j != n_traits_types; ++j) {

            if (0 == j && StringIds::UChar == char_types [i]) {
                // std::char_traits can only be instantiated on
                // char and wchar_t, only UserTraits may be used
                // with UserChar
                continue;
            }

            if (_rw_opt_traits_types [j] < 0) {
                // issue only the first note
                rw_note (-1 > _rw_opt_traits_types [j]--,
                         _rw_this_file, __LINE__,
                         "%s tests disabled", _rw_traits_names [j]);
                continue;
            }

            for (size_t k = 0; k != n_alloc_types; ++k) {

                if (_rw_opt_alloc_types [k] < 0) {
                    // issue only the first note
                    rw_note (-1 > _rw_opt_alloc_types [k]--,
                             _rw_this_file, __LINE__,
                             "%s tests disabled", _rw_alloc_names [k]);
                    continue;
                }

                for (size_t m = 0; m != _rw_string_test_count; ++m) {

                    const StringTest& test = _rw_string_tests [m];

                    // create an object uniquely identifying the overload
                    // of the string function exercised by the set of test
                    // cases defined to exercise it
                    const StringFunc func = {
                        char_types [i],
                        traits_types [j],
                        alloc_types [k],
                        test.which
                    };

                    // set the {CLASS}, {FUNC}, and {FUNCSIG} environment
                    // variable to the name of the basic_string specializaton
                    // and the string function being exercised
                    _rw_setvars (func);

                    // determine whether the function is a member function
                    const bool is_member = StringIds::bit_member & test.which;

                    // compute the function overload's 0-based index
                    const size_t siginx = _rw_get_func_inx (test.which);

                    // check if tests of the function overload
                    // have been disabled
                    if (0 == rw_note (0 <= _rw_opt_func [siginx],
                                      _rw_this_file, __LINE__,
                                      "%{?%{$CLASS}::%{;}%{$FUNCSIG} "
                                      "tests disabled", is_member))
                        continue;

                    rw_info (0, 0, 0, "%{?}%{$CLASS}::%{;}%{$FUNCSIG}",
                             is_member);

                    const size_t case_count = test.case_count;

                    // iterate over all test cases for this function overload
                    // invoking the test case handler for each in turn
                    for (size_t n = 0; n != case_count; ++n) {

                        const StringTestCase& tcase = test.cases [n];

                        _rw_test_case (func, tcase, _rw_test_callback);
                    }
                }
            }
        }
    }

    return 0;
}

/**************************************************************************/

_TEST_EXPORT int
rw_run_string_test (int               argc,
                    char             *argv [],
                    const char       *file,
                    const char       *clause,
                    StringTestFunc   *test_callback,
                    const StringTest *tests,
                    size_t            test_count)
{
    // set the global variables accessed in _rw_run_test
    _rw_test_callback     = test_callback;
    _rw_string_tests      = tests;
    _rw_string_test_count = test_count;

    // put together a command line option specification with options
    // to enable and disable tests exercising functions for all known
    // specializations of the functions specified by the test array
    char   *optbuf     = 0;
    size_t  optbufsize = 0;

    rw_asnprintf (&optbuf, &optbufsize,
                  "|-char~ "
                  "|-wchar_t~ "
                  "|-UserChar~ "
                  "|-char_traits~ "
                  "|-UserTraits~ "
                  "|-allocator~ "
                  "|-UserAlloc~ "

                  "|-no-exceptions# "
                  "|-no-exception-safety# "

                  "|-self-ref~ ");

    for (size_t i = 0; i != test_count; ++i) {

        // for each function append a command line option specification
        // to allow to enable or disable it
        rw_asnprintf (&optbuf, &optbufsize, "%{+}|-");
        _rw_sigcat (&optbuf, &optbufsize, 0, tests [i].which);
        rw_asnprintf (&optbuf, &optbufsize, "%{+}~ ");
    }

    RW_ASSERT (test_count <= 32);
    RW_ASSERT (test_count <= MAX_OVERLOADS);

    // process command line arguments run tests
    const int status =
        rw_test (argc, argv, file, clause,
                 0,              // comment
                 _rw_run_test,   // test callback
                 optbuf,         // option specification

                 // handlers controlling specializations of the template
                 _rw_opt_char_types + 0,
                 _rw_opt_char_types + 1,
                 _rw_opt_char_types + 2,
                 _rw_opt_traits_types + 0,
                 _rw_opt_traits_types + 1,
                 _rw_opt_alloc_types + 0,
                 _rw_opt_alloc_types + 1,

                 // handlers controlling exceptions
                 &_rw_opt_no_exceptions,
                 &_rw_opt_no_exception_safety,

                 // handler controlling self-referential modifiers
                 &_rw_opt_self_ref,

                 // handlers for up to 32 overloads
                 _rw_opt_func +  0,
                 _rw_opt_func +  1,
                 _rw_opt_func +  2,
                 _rw_opt_func +  3,
                 _rw_opt_func +  4,
                 _rw_opt_func +  5,
                 _rw_opt_func +  6,
                 _rw_opt_func +  7,
                 _rw_opt_func +  8,
                 _rw_opt_func +  9,
                 _rw_opt_func + 10,
                 _rw_opt_func + 11,
                 _rw_opt_func + 12,
                 _rw_opt_func + 13,
                 _rw_opt_func + 14,
                 _rw_opt_func + 15,
                 _rw_opt_func + 16,
                 _rw_opt_func + 17,
                 _rw_opt_func + 18,
                 _rw_opt_func + 19,
                 _rw_opt_func + 20,
                 _rw_opt_func + 21,
                 _rw_opt_func + 22,
                 _rw_opt_func + 23,
                 _rw_opt_func + 24,
                 _rw_opt_func + 25,
                 _rw_opt_func + 26,
                 _rw_opt_func + 27,
                 _rw_opt_func + 28,
                 _rw_opt_func + 29,
                 _rw_opt_func + 30,
                 _rw_opt_func + 31,

                 // sentinel
                 (void*)0);

    // free storage allocated for the option specification
    free (optbuf);

    return status;
}
