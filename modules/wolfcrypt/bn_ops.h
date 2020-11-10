#include <cryptofuzz/components.h>
#include <cryptofuzz/operations.h>
#include <cryptofuzz/util.h>
#include <array>

extern "C" {
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/integer.h>
#include <wolfssl/wolfcrypt/ecc.h>
#if defined(WOLFSSL_SP_MATH)
 #include <wolfssl/wolfcrypt/sp.h>
#endif
}

namespace cryptofuzz {
namespace module {
namespace wolfCrypt_bignum {

class Bignum {
    private:
        mp_int* mp = nullptr;
        Datasource& ds;
        bool noFree = false;
    public:

        Bignum(Datasource& ds) :
            ds(ds) {
            mp = (mp_int*)util::malloc(sizeof(mp_int));
            if ( mp_init(mp) != MP_OKAY ) {
                util::free(mp);
                throw std::exception();
            }
        }

        Bignum(mp_int* mp, Datasource& ds) :
            mp(mp),
            ds(ds),
            noFree(true)
        { }

        ~Bignum() {
            if ( noFree == false ) {
                /* noret */ mp_clear(mp);
                util::free(mp);
            }
        }

        Bignum(const Bignum& other) :
            ds(other.ds) {
            mp = (mp_int*)util::malloc(sizeof(mp_int));
            if ( mp_init(mp) != MP_OKAY ) {
                util::free(mp);
                throw std::exception();
            }
            if ( mp_copy(other.mp, mp) != MP_OKAY ) {
                util::free(mp);
                throw std::exception();
            }
        }

        Bignum(const Bignum&& other) :
            ds(other.ds) {
            mp = (mp_int*)util::malloc(sizeof(mp_int));
            if ( mp_init(mp) != MP_OKAY ) {
                util::free(mp);
                throw std::exception();
            }
            if ( mp_copy(other.mp, mp) != MP_OKAY ) {
                util::free(mp);
                throw std::exception();
            }
        }

        void SetNoFree(void) {
            noFree = true;
        }

        bool Set(const std::string s) {
            bool ret = false;

            bool hex = false;
            try {
                hex = ds.Get<bool>();
            } catch ( ... ) { }

#if defined(WOLFSSL_SP_MATH)
            hex = true;
#endif

            if ( hex == true ) {
                const auto asDec = util::DecToHex(s);
                CF_CHECK_EQ(mp_read_radix(mp, asDec.c_str(), 16), MP_OKAY);
            } else {
                CF_CHECK_EQ(mp_read_radix(mp, s.c_str(), 10), MP_OKAY);
            }

            ret = true;
end:
            return ret;
        }

        bool Set(const component::Bignum i) {
            bool ret = false;

            CF_CHECK_EQ(Set(i.ToString()), true);

            ret = true;
end:
            return ret;
        }

        mp_int* GetPtr(void) const {
            return mp;
        }

        std::optional<uint64_t> AsUint64(void) const {
            std::optional<uint64_t> ret = std::nullopt;
            uint64_t v = 0;

#if !defined(WOLFSSL_SP_MATH)
            CF_CHECK_EQ(mp_isneg(mp), 0);
#endif
            CF_CHECK_LTE(mp_count_bits(mp), (int)(sizeof(v) * 8));
            CF_CHECK_EQ(mp_to_unsigned_bin_len(mp, (uint8_t*)&v, sizeof(v)), MP_OKAY);
            v =
                ((v & 0xFF00000000000000) >> 56) |
                ((v & 0x00FF000000000000) >> 40) |
                ((v & 0x0000FF0000000000) >> 24) |
                ((v & 0x000000FF00000000) >>  8) |
                ((v & 0x00000000FF000000) <<  8) |
                ((v & 0x0000000000FF0000) << 24) |
                ((v & 0x000000000000FF00) << 40) |
                ((v & 0x00000000000000FF) << 56);

            ret = v;
end:
            return ret;
        }

        template <class T>
        std::optional<T> AsUnsigned(void) const {
            std::optional<T> ret = std::nullopt;
            T v2;

            auto v = AsUint64();
            CF_CHECK_NE(v, std::nullopt);

            v2 = *v;
            CF_CHECK_EQ(v2, *v);

            ret = v2;

end:
            return ret;
        }

        std::optional<std::string> ToDecString(void) {
            std::optional<std::string> ret = std::nullopt;
            char* str = nullptr;


#if defined(WOLFSSL_SP_MATH)
            str = (char*)util::malloc(8192);

            CF_CHECK_EQ(mp_tohex(mp, str), MP_OKAY);
            ret = { util::HexToDec(str) };
#else
            bool hex = false;
            int size;

            try {
                hex = ds.Get<bool>();
            } catch ( ... ) { }


            if ( hex == true ) {
                CF_CHECK_EQ(mp_radix_size(mp, 16, &size), MP_OKAY);
                str = (char*)util::malloc(size+1);

                CF_CHECK_EQ(mp_tohex(mp, str), MP_OKAY);
                ret = { util::HexToDec(str) };
            } else {
                CF_CHECK_EQ(mp_radix_size(mp, 10, &size), MP_OKAY);
                str = (char*)util::malloc(size);

                CF_CHECK_EQ(mp_toradix(mp, str, 10), MP_OKAY);
                ret = std::string(str);
            }
#endif

end:
            free(str);

            return ret;
        }

        std::optional<component::Bignum> ToComponentBignum(void) {
            std::optional<component::Bignum> ret = std::nullopt;

            auto str = ToDecString();
            CF_CHECK_NE(str, std::nullopt);
            ret = { str };
end:
            return ret;
        }

        bool ToBin(uint8_t* dest, const size_t size) {
            bool ret = false;

            CF_CHECK_EQ(mp_to_unsigned_bin_len(GetPtr(), dest, size), MP_OKAY);

            ret = true;
end:
            return ret;
        }


        static std::optional<std::vector<uint8_t>> ToBin(Datasource& ds, const component::Bignum b, std::optional<size_t> size = std::nullopt) {
            std::optional<std::vector<uint8_t>> ret = std::nullopt;
            std::vector<uint8_t> v;
            Bignum bn(ds);

            CF_CHECK_EQ(bn.Set(b), true);
            if ( size != std::nullopt ) {
                v.resize(*size);
            } else {
                v.resize( mp_unsigned_bin_size(bn.GetPtr()) );
            }
            CF_CHECK_EQ(bn.ToBin(v.data(), v.size()), true);

            ret = v;
end:
            return ret;
        }

        static bool ToBin(Datasource& ds, const component::Bignum b, uint8_t* dest, const size_t size) {
            bool ret = false;
            Bignum bn(ds);

            CF_CHECK_EQ(bn.Set(b), true);
            CF_CHECK_EQ(bn.ToBin(dest, size), true);

            ret = true;
end:
            return ret;
        }

        static bool ToBin(Datasource& ds, const component::BignumPair b, uint8_t* dest, const size_t size) {
            if ( (size % 2) != 0 ) {
                abort();
            }
            bool ret = false;
            const auto halfSize = size / 2;

            CF_CHECK_EQ(ToBin(ds, b.first, dest, halfSize), true);
            CF_CHECK_EQ(ToBin(ds, b.second, dest + halfSize, halfSize), true);

            ret = true;
end:
            return ret;
        }

        static std::optional<component::Bignum> BinToBignum(Datasource& ds, const uint8_t* src, const size_t size) {
            std::optional<component::Bignum> ret = std::nullopt;

            wolfCrypt_bignum::Bignum bn(ds);
            CF_CHECK_EQ(mp_read_unsigned_bin(bn.GetPtr(), src, size), MP_OKAY);

            ret = bn.ToComponentBignum();

end:
            return ret;
        }

        static std::optional<component::BignumPair> BinToBignumPair(Datasource& ds, const uint8_t* src, const size_t size) {
            if ( (size % 2) != 0 ) {
                abort();
            }
            std::optional<component::BignumPair> ret = std::nullopt;
            std::optional<component::Bignum> A, B;
            const auto halfSize = size / 2;

            {
                wolfCrypt_bignum::Bignum bn(ds);
                CF_CHECK_EQ(mp_read_unsigned_bin(bn.GetPtr(), src, halfSize), MP_OKAY);
                CF_CHECK_NE(A = bn.ToComponentBignum(), std::nullopt);
            }

            {
                wolfCrypt_bignum::Bignum bn(ds);
                CF_CHECK_EQ(mp_read_unsigned_bin(bn.GetPtr(), src + halfSize, halfSize), MP_OKAY);
                CF_CHECK_NE(B = bn.ToComponentBignum(), std::nullopt);
            }


            ret = {A->ToTrimmedString(), B->ToTrimmedString()};

end:
            return ret;
        }

        inline bool operator==(const Bignum& rhs) const {
            return mp_cmp(GetPtr(), rhs.GetPtr()) == MP_EQ;
        }
};

class BignumCluster {
    private:
        Datasource& ds;
        std::array<Bignum, 4> bn;
    public:
        BignumCluster(Datasource& ds, Bignum bn0, Bignum bn1, Bignum bn2, Bignum bn3) :
            ds(ds),
            bn({bn0, bn1, bn2, bn3})
        { }

        Bignum& operator[](const size_t index) {
            if ( index >= bn.size() ) {
                abort();
            }

            try {
                /* Rewire? */
                if ( ds.Get<bool>() == true ) {
                    /* Pick a random bignum */
                    const auto newIndex = ds.Get<uint8_t>() % 4;

                    /* Same value? */
                    if ( bn[newIndex] == bn[index] ) {
                        /* Then return reference to other bignum */
                        return bn[newIndex];
                    }

                    /* Fall through */
                }
            } catch ( fuzzing::datasource::Datasource::OutOfData ) { }

            return bn[index];
        }

        bool Set(const size_t index, const std::string s) {
            if ( index >= bn.size() ) {
                abort();
            }

            return bn[index].Set(s);
        }

        mp_int* GetDestPtr(const size_t index) {
            return bn[index].GetPtr();
        }
};

class Operation {
    public:
        virtual bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const = 0;
        virtual ~Operation() { }
};

class Add : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Sub : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Mul : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Div : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class ExpMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Sqr : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class GCD : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class InvMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Cmp : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Abs : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Neg : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class RShift : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class LShift1 : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsNeg : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsEq : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsZero : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsOne : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class MulMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class AddMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class SubMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class SqrMod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Bit : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class CmpAbs : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class SetBit : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class LCM : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Mod : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsEven : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class IsOdd : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class MSB : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class NumBits : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Set : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Jacobi : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Exp2 : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class NumLSZeroBits : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class MulAdd : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class CondSet : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

class Rand : public Operation {
    public:
        bool Run(Datasource& ds, Bignum& res, BignumCluster& bn) const override;
};

} /* namespace wolfCrypt_bignum */
} /* namespace module */
} /* namespace cryptofuzz */
