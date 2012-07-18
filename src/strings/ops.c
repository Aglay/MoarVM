#include "moarvm.h"

#define GRAPHS_EQUAL(d1, d2, g) (memcmp(d1, d2, g * sizeof(MVMuint32)) == 0)

/* Compares two strings for equality. */
MVMint64 MVM_string_equal(MVMThreadContext *tc, MVMString *a, MVMString *b) {
    if (a->body.graphs != b->body.graphs)
        return 0;
    return (MVMint64)GRAPHS_EQUAL(a->body.data, b->body.data, a->body.graphs);
}

/* Returns the location of one string in another or -1  */
MVMint64 MVM_string_index(MVMThreadContext *tc, MVMString *haystack, MVMString *needle, MVMint64 start) {
    MVMint64 result = -1;
    size_t index    = (size_t)start;
    
    if (!IS_CONCRETE((MVMObject *)haystack)) {
        MVM_exception_throw_adhoc(tc, "index needs a concrete search target");
    }
    
    if (!IS_CONCRETE((MVMObject *)needle)) {
        MVM_exception_throw_adhoc(tc, "index needs a concrete search term");
    }
    
    if (!needle->body.graphs && !haystack->body.graphs)
        return 0; /* the empty strings are equal and start at zero */
    
    if (!haystack->body.graphs)
        return -1;
    
    if (start < 0 || start >= haystack->body.graphs)
        /* maybe return -1 instead? */
        MVM_exception_throw_adhoc(tc, "index start offset out of range");
    
    if (needle->body.graphs > haystack->body.graphs || needle->body.graphs < 1)
        return -1;
    /* brute force for now. */
    while (index <= haystack->body.graphs - needle->body.graphs) {
        if (GRAPHS_EQUAL(needle->body.data, haystack->body.data + index, needle->body.graphs)) {
            result = (MVMint64)index;
            break;
        }
        index++;
    }
    return result;
}

/* Returns a substring of the given string */
MVMString * MVM_string_substring(MVMThreadContext *tc, MVMString *a, MVMint64 start, MVMint64 length) {
    MVMString *result;
    
    if (start < 0) {
        start += a->body.graphs;
        if (start < 0)
            start = 0;
    }
    
    if (!IS_CONCRETE((MVMObject *)a)) {
        MVM_exception_throw_adhoc(tc, "Substring needs a concrete string");
    }
    
    if (start >= a->body.graphs)
        MVM_exception_throw_adhoc(tc, "Substring start offset (%lld) cannot be past end of string (%lld)",
            start, a->body.graphs);
    
    if (length < -1) /* -1 signifies go to the end of the string */
        MVM_exception_throw_adhoc(tc, "Substring length (%lld) cannot be negative", length);
    
    if (length == -1)
        length = a->body.graphs - start;
    
    if (start + length > a->body.graphs)
        MVM_exception_throw_adhoc(tc, "Substring end (%lld) cannot be past end of string (%lld)",
            start + length, a->body.graphs);
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&a);
    result = (MVMString *)REPR(a)->allocate(tc, STABLE(a));
    MVM_gc_root_temp_pop(tc);
    
    result->body.codes  = length; /* XXX TODO this is wrong */
    result->body.graphs = length;
    
    if (length > 0) {
        result->body.data = malloc(sizeof(MVMint32) * length);
        memcpy(result->body.data, a->body.data + start, sizeof(MVMint32) * length);
    }
    
    return result;
}

MVMString * MVM_string_concatenate(MVMThreadContext *tc, MVMString *a, MVMString *b) {
    MVMString *result;
    
    if (!IS_CONCRETE((MVMObject *)a) || !IS_CONCRETE((MVMObject *)b)) {
        MVM_exception_throw_adhoc(tc, "Concatenate needs concrete strings");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&a);
    result = (MVMString *)REPR(a)->allocate(tc, STABLE(a));
    MVM_gc_root_temp_pop(tc);
    
    /* there could be unattached combining chars at the beginning of b,
       so, XXX TODO handle this */
    result->body.codes  = a->body.codes + b->body.codes;
    result->body.graphs = a->body.graphs + b->body.graphs;
    
    if (result->body.graphs) {
        result->body.data = malloc(sizeof(MVMint32) * result->body.graphs);
        if (a->body.graphs)
            memcpy(result->body.data, a->body.data, sizeof(MVMint32) * a->body.graphs);
        if (b->body.graphs)
            memcpy(result->body.data + a->body.graphs, b->body.data, sizeof(MVMint32) * b->body.graphs);
    }
    
    return result;
}

MVMString * MVM_string_repeat(MVMThreadContext *tc, MVMString *a, MVMint64 count) {
    MVMString *result;
    size_t i = 0;
    
    if (!IS_CONCRETE((MVMObject *)a)) {
        MVM_exception_throw_adhoc(tc, "repeat needs a concrete string");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&a);
    result = (MVMString *)REPR(a)->allocate(tc, STABLE(a));
    MVM_gc_root_temp_pop(tc);
    
    if (count < 0)
        MVM_exception_throw_adhoc(tc, "repeat count (%lld) cannot be negative", count);
    
    result->body.codes  = a->body.codes * count;
    result->body.graphs = a->body.graphs * count;
    
    if (count) {
        result->body.data = malloc(sizeof(MVMint32) * result->body.graphs);
        while (count--)
            memcpy(result->body.data + (i++ * a->body.graphs), a->body.data, sizeof(MVMint32) * a->body.graphs);
    }
    
    return result;
}

void MVM_string_say(MVMThreadContext *tc, MVMString *a) {
    MVMuint8 *utf8_encoded;
    MVMuint64 utf8_encoded_length;
    
    if (!IS_CONCRETE((MVMObject *)a)) {
        MVM_exception_throw_adhoc(tc, "say needs a concrete string");
    }
    
    utf8_encoded = MVM_string_utf8_encode(tc, a, &utf8_encoded_length);
    
    fwrite(utf8_encoded, 1, utf8_encoded_length, stdout);
    printf("\n");
    
    free(utf8_encoded);
}

/* Tests whether one string a has the other string b as a substring at that index */
MVMint64 MVM_string_equal_at(MVMThreadContext *tc, MVMString *a, MVMString *b, MVMint64 offset) {
    
    if (!IS_CONCRETE((MVMObject *)a) || !IS_CONCRETE((MVMObject *)b)) {
        MVM_exception_throw_adhoc(tc, "equal_at needs concrete strings");
    }
    
    if (a->body.graphs < b->body.graphs)
        return 0;
    if (offset < 0) {
        offset += a->body.graphs;
        if (offset < 0)
            offset = 0; /* XXX I think this is the right behavior here */
    }
    return (MVMint64)GRAPHS_EQUAL(a->body.data + (size_t)offset, b->body.data, b->body.graphs);
}

/* more general form of has_at; compares two substrings for equality */
MVMint64 MVM_string_have_at(MVMThreadContext *tc, MVMString *a,
        MVMint64 starta, MVMint64 length, MVMString *b, MVMint64 startb) {
    
    if (!IS_CONCRETE((MVMObject *)a) || !IS_CONCRETE((MVMObject *)b)) {
        MVM_exception_throw_adhoc(tc, "have_at needs concrete strings");
    }
    
    if (starta < 0 || startb < 0)
        return 0;
    if (length == 0)
        return 1;
    if (starta + length > a->body.graphs || startb + length > b->body.graphs)
        return 0;
    return (MVMint64)GRAPHS_EQUAL(a->body.data + (size_t)starta,
            b->body.data + (size_t)startb, (size_t)length);
}

/* returns the codepoint (could be a negative synthetic) at a given index of the string */
MVMint64 MVM_string_get_codepoint_at(MVMThreadContext *tc, MVMString *a, MVMint64 index) {
    
    if (!IS_CONCRETE((MVMObject *)a)) {
        MVM_exception_throw_adhoc(tc, "codepoint_at needs a concrete string");
    }
    
    if (index < 0 || index >= a->body.graphs)
        MVM_exception_throw_adhoc(tc, "Invalid string index: max %lld, got %lld",
            index >= a->body.graphs - 1, index);
    return (MVMint64)a->body.data[index];
}

/* finds the location of a codepoint in a string.  Useful for small character class lookup */
MVMint64 MVM_string_index_of_codepoint(MVMThreadContext *tc, MVMString *a, MVMint64 codepoint) {
    size_t index = -1;
    while (++index < a->body.graphs)
        if (a->body.data[index] == codepoint)
            return index;
    return -1;
}

/* Uppercases a string. */
MVMString * MVM_string_uc(MVMThreadContext *tc, MVMString *s) {
    MVMString *result;
    MVMuint64 i;
    
    if (!IS_CONCRETE((MVMObject *)s)) {
        MVM_exception_throw_adhoc(tc, "uc needs a concrete string");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&s);
    result = (MVMString *)REPR(s)->allocate(tc, STABLE(s));
    MVM_gc_root_temp_pop(tc);
    
    /* XXX Need to handle cases where character count varies. */
    result->body.codes  = s->body.codes;
    result->body.graphs = s->body.graphs;
    result->body.data = malloc(sizeof(MVMint32) * result->body.graphs);
    for (i = 0; i < s->body.graphs; i++) {
        MVMCodePoint *cp = MVM_unicode_codepoint_info(tc, s->body.data[i]);
        result->body.data[i] = cp ? cp->uc : s->body.data[i];
    }

    return result;
}

/* Lowercases a string. */
MVMString * MVM_string_lc(MVMThreadContext *tc, MVMString *s) {
    MVMString *result;
    MVMuint64 i;
    
    if (!IS_CONCRETE((MVMObject *)s)) {
        MVM_exception_throw_adhoc(tc, "lc needs a concrete string");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&s);
    result = (MVMString *)REPR(s)->allocate(tc, STABLE(s));
    MVM_gc_root_temp_pop(tc);
    
    /* XXX Need to handle cases where character count varies. */
    result->body.codes  = s->body.codes;
    result->body.graphs = s->body.graphs;
    result->body.data = malloc(sizeof(MVMint32) * result->body.graphs);
    for (i = 0; i < s->body.graphs; i++) {
        MVMCodePoint *cp = MVM_unicode_codepoint_info(tc, s->body.data[i]);
        result->body.data[i] = cp ? cp->lc : s->body.data[i];
    }

    return result;
}

/* Titlecases a string. */
MVMString * MVM_string_tc(MVMThreadContext *tc, MVMString *s) {
    MVMString *result;
    MVMuint64 i;
    
    if (!IS_CONCRETE((MVMObject *)s)) {
        MVM_exception_throw_adhoc(tc, "tc needs a concrete string");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&s);
    result = (MVMString *)REPR(s)->allocate(tc, STABLE(s));
    MVM_gc_root_temp_pop(tc);
    
    /* XXX Need to handle cases where character count varies. */
    result->body.codes  = s->body.codes;
    result->body.graphs = s->body.graphs;
    result->body.data = malloc(sizeof(MVMint32) * result->body.graphs);
    for (i = 0; i < s->body.graphs; i++) {
        MVMCodePoint *cp = MVM_unicode_codepoint_info(tc, s->body.data[i]);
        result->body.data[i] = cp ? cp->tc : s->body.data[i];
    }

    return result;
}

/* decodes a C buffer to an MVMString, dependent on the encoding type flag */
MVMString * MVM_decode_C_buffer_to_string(MVMThreadContext *tc,
        MVMObject *type_object, char *Cbuf, MVMint64 byte_length, MVMint64 encoding_flag) {
        
    /* someday make 0 mean "try really hard to detect the encoding */
    
    switch(encoding_flag) {
        case MVM_encoding_type_utf8:
            return MVM_string_utf8_decode(tc, type_object, Cbuf, byte_length);
        case MVM_encoding_type_ascii:
            return MVM_string_ascii_decode(tc, type_object, Cbuf, byte_length);
        case MVM_encoding_type_latin1:
            return MVM_string_latin1_decode(tc, type_object, Cbuf, byte_length);
        default:
            MVM_exception_throw_adhoc(tc, "invalid encoding type flag: %d", encoding_flag);
    }
    return NULL;
}

/* encodes an MVMString to a C buffer, dependent on the encoding type flag */
char * MVM_encode_string_to_C_buffer(MVMThreadContext *tc, MVMString *s, MVMint64 start, MVMint64 length, MVMuint64 *output_size, MVMint64 encoding_flag) {
    switch(encoding_flag) {
        case MVM_encoding_type_utf8:
            return MVM_string_utf8_encode_substr(tc, s, output_size, start, length);
        case MVM_encoding_type_ascii:
            return MVM_string_ascii_encode_substr(tc, s, output_size, start, length);
        case MVM_encoding_type_latin1:
            return MVM_string_latin1_encode_substr(tc, s, output_size, start, length);
        default:
            MVM_exception_throw_adhoc(tc, "invalid encoding type flag: %d", encoding_flag);
    }
    return NULL;
}

MVMObject * MVM_string_split(MVMThreadContext *tc, MVMString *input, MVMObject *type_object, MVMString *separator) {
    MVMObject *result;
    MVMint64 start, end, sep_length;
    
    if (!IS_CONCRETE((MVMObject *)separator)) {
        MVM_exception_throw_adhoc(tc, "split needs a concrete string separator");
    }
    
    if (REPR(type_object)->ID != MVM_REPR_ID_MVMArray || IS_CONCRETE(type_object)) {
        MVM_exception_throw_adhoc(tc, "split needs a type object with MVMArray REPR");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&type_object);
    result = REPR(type_object)->allocate(tc, STABLE(type_object));
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&result);
    
    start = 0;
    end = input->body.graphs;
    sep_length = separator->body.graphs;
    
    while (start < end) {
        MVMString *portion;
        MVMint64 index, length;
        
        index = MVM_string_index(tc, input, separator, start);
        length = sep_length ? (index == -1 ? end : index) - start : 1;
        //printf("got start %d index %d length %d\n", start, index, length);
        portion = MVM_string_substring(tc, input, start, length);
        start += length + sep_length;
        
        if (portion->body.graphs) {
            REPR(result)->pos_funcs->push_boxed(tc, STABLE(result),
                result, OBJECT_BODY(result), (MVMObject *)portion);
        }
    }
    
    MVM_gc_root_temp_pop_n(tc, 2);
    
    return result;
}

MVMString * MVM_string_join(MVMThreadContext *tc, MVMObject *input, MVMString *separator) {
    MVMint64 elems, length = 0, index = -1, position;
    MVMString *portion, *result;
    
    if (REPR(input)->ID != MVM_REPR_ID_MVMArray || !IS_CONCRETE(input)) {
        MVM_exception_throw_adhoc(tc, "join needs a concrete object with MVMArray REPR");
    }
    
    if (!IS_CONCRETE((MVMObject *)separator)) {
        MVM_exception_throw_adhoc(tc, "join needs a concrete separator");
    }
    
    MVM_gc_root_temp_push(tc, (MVMCollectable **)&separator);
    result = (MVMString *)(REPR(separator)->allocate(tc, STABLE(separator)));
    MVM_gc_root_temp_pop(tc);
    
    elems = REPR(input)->pos_funcs->elems(tc, STABLE(input),
        input, OBJECT_BODY(input));
    
    while (++index < elems) {
        MVMObject *item = REPR(input)->pos_funcs->at_pos_boxed(tc, STABLE(input),
            input, OBJECT_BODY(input), index);
        
        /* allow null items in the array, I guess.. */
        if (!item)
            continue;
        if (REPR(item)->ID != MVM_REPR_ID_MVMString || !IS_CONCRETE((MVMObject *)item))
            MVM_exception_throw_adhoc(tc, "join needs concrete strings only");
        
        portion = (MVMString *)item;
        length += portion->body.graphs + (index ? separator->body.graphs : 0);
    }
    
    result->body.graphs = length;
    result->body.codes = length; /* XXX NFG wrong */
    if (length)
        result->body.data = malloc(sizeof(MVMint32) * length);
    
    index = -1;
    position = 0;
    
    while (++index < elems) {
        MVMObject *item = REPR(input)->pos_funcs->at_pos_boxed(tc, STABLE(input),
            input, OBJECT_BODY(input), index);
        
        if (!item)
            continue;
        
        if (index && separator->body.graphs) {
            memcpy(result->body.data + position, separator->body.data, sizeof(MVMint32) * separator->body.graphs);
            position += separator->body.graphs;
        }
        
        portion = (MVMString *)item;
        length = portion->body.graphs;
        
        memcpy(result->body.data + position, portion->body.data, sizeof(MVMint32) * length);
        
        position += length;
    }
    if (result->body.graphs != position)
        MVM_exception_throw_adhoc(tc, "join had an internal error");
    
    return result;
}
