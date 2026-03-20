#pragma once
#include<cppp/bytearray.hpp>
#include<cppp/int.hpp>
#include<type_traits>
#include<algorithm>
#include<stdexcept>
#include<concepts>
#include<utility>
#include<cstdint>
namespace x86{
    using namespace cppp::literals;
    using cppp::bytes;
    enum class width : std::uint8_t{
        BYTE = 0, WORD = 1, DWORD = 2, QWORD = 3,
        W8 = 0, W16 = 1, W32 = 2, W64 = 3
    };
    constexpr inline std::uint32_t width_to_byte_count(width w){
        return w==width::W8?1:w==width::W16?2:w==width::W32?4:8;
    }
    enum class scale : std::uint8_t{
        S1 = 0, S2 = 1, S4 = 2, S8 = 3
    };
    template<width w>
    using wvx = std::conditional_t<
        w == width::BYTE,
        std::uint8_t,
        std::conditional_t<
            w == width::WORD,
            std::uint16_t,
            std::enable_if_t<
            w == width::DWORD,
                std::uint32_t
            >
        >
    >;
    template<width w>
    using wv = std::conditional_t<
        w == width::QWORD,
        std::uint64_t,
        wvx<w>
    >;
    template<width w>
    using wvs = std::make_signed_t<wv<w>>;
    namespace reg{
        constexpr inline std::byte A = 0_b;
        constexpr inline std::byte C = 1_b;
        constexpr inline std::byte D = 2_b;
        constexpr inline std::byte B = 3_b;
        constexpr inline std::byte SP = 4_b;
        constexpr inline std::byte BP = 5_b;
        constexpr inline std::byte SI = 6_b;
        constexpr inline std::byte DI = 7_b;
        constexpr inline std::byte AH = 4_b;
        constexpr inline std::byte BH = 5_b;
        constexpr inline std::byte CH = 6_b;
        constexpr inline std::byte DH = 7_b;
        template<std::uint32_t ordinal> requires(ordinal < 8)
        inline std::byte XMM = static_cast<std::byte>(ordinal);
    }
    namespace rex{
        constexpr inline std::byte B = 0b0001_b;
        constexpr inline std::byte X = 0b0010_b;
        constexpr inline std::byte R = 0b0100_b;
        constexpr inline std::byte W = 0b1000_b;
    }
    namespace detail{
        template<typename ...T>
        struct pack{};
        template<typename T,typename U>
        struct pack_concat{};
        template<typename ...Ts,typename ...Us>
        struct pack_concat<pack<Ts...>,pack<Us...>>{
            using type = pack<Ts...,Us...>;
        };
        template<typename T,typename U>
        using pack_concat_t = pack_concat<T,U>::type;
        template<typename ...T>
        struct pack_concat_n{
            using type = pack<>;
        };
        template<typename P,typename ...T>
        struct pack_concat_n<P,T...>{
            using type = pack_concat_t<P,typename pack_concat_n<T...>::type>;
        };
        template<typename ...T>
        using pack_concat_n_t = pack_concat_n<T...>::type;
        template<typename T,template<typename,typename> typename Op,typename U>
        struct pack_apply2{};
        template<typename ...Ts,template<typename,typename> typename Op,typename U>
        struct pack_apply2<pack<Ts...>,Op,U>{
            using type = pack<typename Op<Ts,U>::type...>;
        };
        template<typename T,template<typename,typename> typename Op,typename U>
        using pack_apply2_t = pack_apply2<T,Op,U>::type;
        template<typename T,typename U,template<typename,typename> typename Op>
        struct outer_product{};
        template<typename T,typename ...Us,template<typename,typename> typename Op>
        struct outer_product<T,pack<Us...>,Op>{
            using type = pack_concat_n_t<pack_apply2_t<T,Op,Us>...>;
        };
        template<typename T,typename U,template<typename,typename> typename Op>
        using outer_product_t = outer_product<T,U,Op>::type;
    }
    template<std::byte rexb,std::byte ...op>
    struct opcode{
        using overloads = detail::pack<detail::pack<>>;
        static void encode(bytes& b){
            if constexpr(rexb != 0_b){
                b.append(rexb | 0b0100'0000_b);
            }
            (..., b.append(op));
        }
    };
    template<std::byte ...b>
    using opcode_nr = opcode<0_b,b...>;
    template<std::byte pref>
    struct prefix{
        using overloads = detail::pack<detail::pack<>>;
        static void encode(bytes& b){
            b.append(pref);
        }
    };
    template<width w>
    struct immediate{
        using overloads = detail::pack<detail::pack<wv<w>>>;
        static void encode(bytes& b,wv<w> imm){
            b.appendl(imm);
        }
    };
    // Not an instruction component! used for overload resolution when encoding modrm
    template<width w>
    struct displacement{
        wvs<w> amount;
    };
    constexpr static std::byte MODRM_R_DST = 0xff_b;
    constexpr static std::byte MODRM_R_SRC = 0xfe_b;
    namespace detail{
        template<std::byte rmreg,width Dw>
        struct modrm_with_disp{
            static void encode(bytes& b,std::byte mod,std::byte rm,displacement<Dw> disp){
                b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
            }
        };
        template<width Dw>
        struct modrm_rdst_with_disp{
            static void encode(bytes& b,std::byte rmreg,std::byte mod,std::byte rm,displacement<Dw> disp){
                b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
            }
        };
        template<width Dw>
        struct modrm_rsrc_with_disp{
            static void encode(bytes& b,std::byte mod,std::byte rm,displacement<Dw> disp,std::byte rmreg){
                b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
            }
        };
    }
    template<std::byte rmreg>
    struct modrm : detail::modrm_with_disp<rmreg,width::W8>,
                   detail::modrm_with_disp<rmreg,width::W16>,
                   detail::modrm_with_disp<rmreg,width::W32>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W8>>,
            detail::pack<std::byte,std::byte,displacement<width::W16>>,
            detail::pack<std::byte,std::byte,displacement<width::W32>>
        >;
        using detail::modrm_with_disp<rmreg,width::W8>::encode;
        using detail::modrm_with_disp<rmreg,width::W16>::encode;
        using detail::modrm_with_disp<rmreg,width::W32>::encode;
        static void encode(bytes& b,std::byte mod,std::byte rm){
            b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
        }
    };
    template<>
    struct modrm<MODRM_R_DST> : detail::modrm_rdst_with_disp<width::W8>,
                                detail::modrm_rdst_with_disp<width::W16>,
                                detail::modrm_rdst_with_disp<width::W32>,
                                detail::modrm_rdst_with_disp<width::W64>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte,std::byte>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W8>>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W16>>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W32>>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W64>>
        >;
        using detail::modrm_rdst_with_disp<width::W8>::encode;
        using detail::modrm_rdst_with_disp<width::W16>::encode;
        using detail::modrm_rdst_with_disp<width::W32>::encode;
        using detail::modrm_rdst_with_disp<width::W64>::encode;
        static void encode(bytes& b,std::byte rmreg,std::byte mod,std::byte rm){
            b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
        }
    };
    template<>
    struct modrm<MODRM_R_SRC> : detail::modrm_rsrc_with_disp<width::W8>,
                                detail::modrm_rsrc_with_disp<width::W16>,
                                detail::modrm_rsrc_with_disp<width::W32>,
                                detail::modrm_rsrc_with_disp<width::W64>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W8>,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W16>,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W32>,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W64>,std::byte>
        >;
        using detail::modrm_rsrc_with_disp<width::W8>::encode;
        using detail::modrm_rsrc_with_disp<width::W16>::encode;
        using detail::modrm_rsrc_with_disp<width::W32>::encode;
        using detail::modrm_rsrc_with_disp<width::W64>::encode;
        static void encode(bytes& b,std::byte mod,std::byte rm,std::byte rmreg){
            b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
        }
    };
    namespace detail{
        template<typename Comp,typename Ol,typename Rest,typename RestOl>
        struct encode_comp_sig{};
        template<typename Comp,typename ...Argv,typename Rest,typename ...RestArgv>
        struct encode_comp_sig<Comp,pack<Argv...>,Rest,pack<RestArgv...>>{
            static void encode(bytes& b,Argv ...a,RestArgv ...r){
                Comp::encode(b,static_cast<Argv>(a)...);
                Rest::encode(b,static_cast<RestArgv>(r)...);
            }
        };
        template<typename Comp,typename Olp,typename Rest,typename RestOl>
        struct encode_comp_for_one_rol{};
        template<typename Comp,typename ...Ols,typename Rest,typename RestOl>
        struct encode_comp_for_one_rol<Comp,pack<Ols...>,Rest,RestOl> : encode_comp_sig<Comp,Ols,Rest,RestOl>...{
            using encode_comp_sig<Comp,Ols,Rest,RestOl>::encode...;
        };
        template<typename Comp,typename Olp,typename Rest,typename RestOlp>
        struct encode_comp{};
        template<typename Comp,typename Olp,typename Rest,typename ...RestOls>
        struct encode_comp<Comp,Olp,Rest,pack<RestOls...>> : encode_comp_for_one_rol<Comp,Olp,Rest,RestOls>...{
            using encode_comp_for_one_rol<Comp,Olp,Rest,RestOls>::encode...;
        };
    }
    
    template<typename ...C>
    struct InstructionEncoding{
        using overloads = detail::pack<detail::pack<>>;
        static void encode(bytes&){}
    };
    template<typename Comp,typename ...Rest>
    struct InstructionEncoding<Comp,Rest...> : detail::encode_comp<Comp,typename Comp::overloads,InstructionEncoding<Rest...>,typename InstructionEncoding<Rest...>::overloads>{
        using overloads = detail::outer_product_t<typename Comp::overloads,typename InstructionEncoding<Rest...>::overloads,detail::pack_concat>;
    };

    namespace detail{
        template<width w,typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        struct select_by_width{};
        template<typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        struct select_by_width<width::W8,Ie8,Ie16,Ie32,Ie64>{ using type = Ie8; };
        template<typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        struct select_by_width<width::W16,Ie8,Ie16,Ie32,Ie64>{ using type = Ie16; };
        template<typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        struct select_by_width<width::W32,Ie8,Ie16,Ie32,Ie64>{ using type = Ie32; };
        template<typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        struct select_by_width<width::W64,Ie8,Ie16,Ie32,Ie64>{ using type = Ie64; };
        template<width w,typename Ie8,typename Ie16,typename Ie32,typename Ie64>
        using select_by_width_t = select_by_width<w,Ie8,Ie16,Ie32,Ie64>::type;
    }
    template<typename Ie8,typename Ie16,typename Ie32,typename Ie64>
    struct InstructionEncodings{
        template<width w> requires(!std::same_as<detail::select_by_width_t<w,Ie8,Ie16,Ie32,Ie64>,void>)
        using for_width = detail::select_by_width_t<w,Ie8,Ie16,Ie32,Ie64>;
    };
    template<template<width> typename Tp>
    struct rie_variable_width{};
    namespace detail{
        template<typename T,width w>
        struct rie_reify_width{
            using type = T;
        };
        template<template<width> typename Tp,width w>
        struct rie_reify_width<rie_variable_width<Tp>,w>{
            using type = Tp<w>;
        };
        template<typename T,width w>
        using rie_reify_width_t = rie_reify_width<T,w>::type;
        template<std::byte rexb,typename T>
        struct insert_rex_prefix{
            using type = T;
        };
        template<std::byte rexb,std::byte existing_rexb,std::byte ...op>
        struct insert_rex_prefix<rexb,opcode<existing_rexb,op...>>{
            using type = opcode<rexb|existing_rexb,op...>;
        };
        template<std::byte rexb,typename T>
        using insert_rex_prefix_t = insert_rex_prefix<rexb,T>::type;
        
        template<typename ...T>
        using with_66h = InstructionEncoding<prefix<0x66_b>,rie_reify_width_t<T,width::W16>...>;
        template<typename ...T>
        using reify_for_32 = InstructionEncoding<rie_reify_width_t<T,width::W32>...>;
        template<typename ...T>
        using with_rexw = InstructionEncoding<insert_rex_prefix_t<rex::W,rie_reify_width_t<T,width::W64>>...>;
    }
    template<typename Ie8,typename ...T>
    using RegularInstructionEncodings = InstructionEncodings<Ie8,detail::with_66h<T...>,detail::reify_for_32<T...>,detail::with_rexw<T...>>;
    
    // only 32 & 64 bit versions, like movzx from 16-bit (or movzw* in AT&T syntax)
    template<typename ...T>
    using RegularLongInstructionEncodings = InstructionEncodings<void,void,detail::reify_for_32<T...>,detail::with_rexw<T...>>;
    namespace instructions{
        namespace detail{
            constexpr std::byte addb(std::byte u,std::byte v){
                return static_cast<std::byte>(static_cast<std::uint8_t>(u)+static_cast<std::uint8_t>(v));
            }
            template<std::byte base>
            struct ins{
                using rm_r = RegularInstructionEncodings<
                    InstructionEncoding<opcode_nr<base>,modrm<MODRM_R_SRC>>,
                    opcode_nr<addb(base,1_b)>,modrm<MODRM_R_SRC>
                >;
                using r_rm = RegularInstructionEncodings<
                    InstructionEncoding<opcode_nr<addb(base,2_b)>,modrm<MODRM_R_DST>>,
                    opcode_nr<addb(base,3_b)>,modrm<MODRM_R_DST>
                >;
            };
            template<std::byte base,std::byte opr>
            struct ins_imm{
                using rm_imm = RegularInstructionEncodings<
                    InstructionEncoding<opcode_nr<base>,modrm<opr>,immediate<width::W8>>,
                    opcode_nr<addb(base,1_b)>,modrm<opr>,rie_variable_width<immediate>
                >;
            };
        }
        struct add : detail::ins<0x00_b>, detail::ins_imm<0x80_b,0_b>{};
        struct sub : detail::ins<0x28_b>, detail::ins_imm<0x80_b,5_b>{};
        struct cmp : detail::ins<0x38_b>, detail::ins_imm<0x80_b,7_b>{};
        struct mov : detail::ins<0x88_b>, detail::ins_imm<0xC6_b,0_b>{};
        
        using lea = RegularInstructionEncodings<void,
            opcode_nr<0x8D_b>,modrm<MODRM_R_DST>
        >;
        namespace detail{
            template<std::byte rmop,std::byte rmreg,std::byte rop>
            struct pushpop{
                using rm = InstructionEncoding<opcode_nr<rmop>,modrm<rmreg>>;
                constexpr static void r64(bytes& buf,std::byte r){
                    buf.append(static_cast<std::byte>(static_cast<std::uint8_t>(rop)+static_cast<std::uint8_t>(r)));
                }
                constexpr static void r16(bytes& buf,std::byte r){
                    buf.append(0x66_b);
                    buf.append(static_cast<std::byte>(static_cast<std::uint8_t>(rop)+static_cast<std::uint8_t>(r)));
                }
            };
        }
        struct push : detail::pushpop<0xFF_b,6_b,0x50_b>{};
        struct pop : detail::pushpop<0x8F_b,0_b,0x58_b>{};
        struct call{
            using rm = InstructionEncoding<opcode_nr<0xFF_b>,modrm<2_b>>;
            constexpr static void rel32(bytes& buf,std::uint32_t rel){
                buf.append(0xE8_b);
                buf.appendl(rel);
            }
        };
        struct ret{
            constexpr static void near(bytes& buf){
                buf.append(0xC3_b);
            }
            constexpr static void far(bytes& buf){
                buf.append(0xCB_b);
            }
        };
        struct set{
            using c = InstructionEncoding<opcode_nr<0x0F_b,0x92_b>,modrm<0_b /* this is ignored, let's just use 0 */>>;
            using b = c;
            using z = InstructionEncoding<opcode_nr<0x0F_b,0x94_b>,modrm<0_b>>;
            using e = z;
            using le = InstructionEncoding<opcode_nr<0x0F_b,0x9E_b>,modrm<0_b>>;
            using ng = le;
        };
        struct movzx{
            using from_b = RegularInstructionEncodings<void,opcode_nr<0x0F_b,0xB6_b>,modrm<MODRM_R_DST>>;
            using from_w = RegularLongInstructionEncodings<opcode_nr<0x0F_b,0xB7_b>,modrm<MODRM_R_DST>>;
        };
        constexpr inline void leave(bytes& buf){
            buf.append(0xC9_b);
        }
    }
}
