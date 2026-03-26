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
    using wv = std::conditional_t<
        w == width::BYTE,
        std::uint8_t,
        std::conditional_t<
            w == width::WORD,
            std::uint16_t,
            std::conditional_t<
                w == width::DWORD,
                std::uint32_t,
                std::conditional_t<
                    w == width::QWORD,
                    std::uint64_t,
                    void
                >
            >
        >
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
    namespace detail{
        template<std::size_t n>
        struct component_feedback{};
    }
    template<typename Comp,std::size_t n>
    struct encoded_component{};
    namespace detail{
        template<template<typename> typename Pred,typename ...Ec>
        struct offset_of_first_of{};
        template<template<typename> typename Pred,typename Comp,std::size_t size,typename ...Rest> requires(Pred<Comp>::value)
        struct offset_of_first_of<Pred,encoded_component<Comp,size>,Rest...>{
            constexpr static std::size_t value{0uz};
        };
        template<template<typename> typename Pred,typename Comp,std::size_t size,typename ...Rest> requires(!Pred<Comp>::value)
        struct offset_of_first_of<Pred,encoded_component<Comp,size>,Rest...>{
            constexpr static std::size_t value{size + offset_of_first_of<Pred,Rest...>::value};
        };
        template<template<typename> typename Pred,typename ...Ec>
        constexpr inline std::size_t offset_of_first_of_v = offset_of_first_of<Pred,Ec...>::value;
    }
    template<typename ...Ec>
    struct encode_feedback;
    template<typename ...Comps,std::size_t ...sizes>
    struct encode_feedback<encoded_component<Comps,sizes>...>{
        constexpr static std::size_t total_size = (... + sizes);
        template<template<typename> typename Pred>
        constexpr static std::size_t offset_of_first = detail::offset_of_first_of_v<Pred,encoded_component<Comps,sizes>...>;
    };
    template<typename C,typename Cr,typename R>
    struct encode_feedback_prepend{};
    template<typename C,std::size_t Cn,typename ...Rs>
    struct encode_feedback_prepend<C,detail::component_feedback<Cn>,encode_feedback<Rs...>>{
        using type = encode_feedback<encoded_component<C,Cn>,Rs...>;
    };
    template<typename C,typename Cr,typename R>
    using encode_feedback_prepend_t = encode_feedback_prepend<C,Cr,R>::type;
    template<std::byte rexb,std::byte ...op>
    struct opcode{
        using overloads = detail::pack<detail::pack<>>;
        static detail::component_feedback<(rexb==0_b?0uz:1uz)+sizeof...(op)> encode(bytes& b){
            if constexpr(rexb != 0_b){
                b.append(rexb | 0b0100'0000_b);
            }
            (..., b.append(op));
            return {};
        }
    };
    template<std::byte ...b>
    using opcode_nr = opcode<0_b,b...>;
    template<std::byte pref>
    struct prefix{
        using overloads = detail::pack<detail::pack<>>;
        static detail::component_feedback<1uz> encode(bytes& b){
            b.append(pref);
            return {};
        }
    };
    // Not an instruction component! Used to indicate that the immediate should be left uninitialized and filled in later
    // Not templated because the immediate size is fixed by the instruction encoding
    struct skip_immediate_t{};
    constexpr inline skip_immediate_t skip_immediate{};
    namespace detail{
        template<typename T>
        struct immediate_of_type{
            using overloads = detail::pack<detail::pack<T>,detail::pack<skip_immediate_t>>;
            static detail::component_feedback<sizeof(T)> encode(bytes& b,T imm){
                b.appendl(imm);
                return {};
            }
            static detail::component_feedback<sizeof(T)> encode(bytes& b,skip_immediate_t){
                b.resb(sizeof(T));
                return {};
            }
        };
    }
    template<typename T>
    struct pred_is_immediate{
        constexpr static bool value = false;
    };
    template<typename T>
    struct pred_is_immediate<detail::immediate_of_type<T>>{
        constexpr static bool value = true;
    };
    template<width w>
    using immediate = detail::immediate_of_type<std::conditional_t<w == width::W64,wvs<width::W32>,wv<w>>>;
    template<width w>
    using absolute_immediate = detail::immediate_of_type<wv<w>>;
    template<width w>
    using signed_immediate = detail::immediate_of_type<std::conditional_t<w == width::W64,wvs<width::W32>,wvs<w>>>;
    // Not an instruction component! Used for overload resolution when encoding modrm
    template<width w>
    struct displacement{
        wvs<w> amount;
    };
    // Not an instruction component! Used to indicate that the displacement should be left uninitialized and filled in later
    template<width w>
    struct skip_displacement_t{};
    template<width w>
    constexpr inline skip_displacement_t<w> skip_displacement{};
    constexpr inline std::byte MODRM_R_DST = 0xff_b;
    constexpr inline std::byte MODRM_R_SRC = 0xfe_b;
    namespace detail{
        template<std::byte rmreg,width dw>
        struct modrm_with_disp{
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte mod,std::byte rm,displacement<dw> disp){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
                return {};
            }
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte mod,std::byte rm,skip_displacement_t<dw>){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.resb(width_to_byte_count(dw));
                return {};
            }
        };
        template<width dw>
        struct modrm_rdst_with_disp{
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte rmreg,std::byte mod,std::byte rm,displacement<dw> disp){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
                return {};
            }
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte rmreg,std::byte mod,std::byte rm,skip_displacement_t<dw>){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.resb(width_to_byte_count(dw));
                return {};
            }
        };
        template<width dw>
        struct modrm_rsrc_with_disp{
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte mod,std::byte rm,displacement<dw> disp,std::byte rmreg){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.appendl(disp.amount);
                return {};
            }
            static detail::component_feedback<1uz+width_to_byte_count(dw)> encode(bytes& b,std::byte mod,std::byte rm,skip_displacement_t<dw>,std::byte rmreg){
                b.append((mod<<6uz) | (rmreg<<3uz) | rm);
                b.resb(width_to_byte_count(dw));
                return {};
            }
        };
    }
    template<std::byte rmreg>
    struct modrm : detail::modrm_with_disp<rmreg,width::W8>,
                   detail::modrm_with_disp<rmreg,width::W16>,
                   detail::modrm_with_disp<rmreg,width::W32>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W8>>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W8>>,
            detail::pack<std::byte,std::byte,displacement<width::W16>>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W16>>,
            detail::pack<std::byte,std::byte,displacement<width::W32>>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W32>>
        >;
        using detail::modrm_with_disp<rmreg,width::W8>::encode;
        using detail::modrm_with_disp<rmreg,width::W16>::encode;
        using detail::modrm_with_disp<rmreg,width::W32>::encode;
        static detail::component_feedback<1uz> encode(bytes& b,std::byte mod,std::byte rm){
            b.append((mod<<6uz) | (rmreg<<3uz) | rm);
            return {};
        }
    };
    template<>
    struct modrm<MODRM_R_DST> : detail::modrm_rdst_with_disp<width::W8>,
                                detail::modrm_rdst_with_disp<width::W16>,
                                detail::modrm_rdst_with_disp<width::W32>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte,std::byte>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W8>>,detail::pack<std::byte,std::byte,std::byte,skip_displacement_t<width::W8>>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W16>>,detail::pack<std::byte,std::byte,std::byte,skip_displacement_t<width::W16>>,
            detail::pack<std::byte,std::byte,std::byte,displacement<width::W32>>,detail::pack<std::byte,std::byte,std::byte,skip_displacement_t<width::W32>>
        >;
        using detail::modrm_rdst_with_disp<width::W8>::encode;
        using detail::modrm_rdst_with_disp<width::W16>::encode;
        using detail::modrm_rdst_with_disp<width::W32>::encode;
        static detail::component_feedback<1uz> encode(bytes& b,std::byte rmreg,std::byte mod,std::byte rm){
            b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
            return {};
        }
    };
    template<>
    struct modrm<MODRM_R_SRC> : detail::modrm_rsrc_with_disp<width::W8>,
                                detail::modrm_rsrc_with_disp<width::W16>,
                                detail::modrm_rsrc_with_disp<width::W32>{
        using overloads = detail::pack<
            detail::pack<std::byte,std::byte,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W8>,std::byte>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W8>,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W16>,std::byte>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W16>,std::byte>,
            detail::pack<std::byte,std::byte,displacement<width::W32>,std::byte>,detail::pack<std::byte,std::byte,skip_displacement_t<width::W32>,std::byte>
        >;
        using detail::modrm_rsrc_with_disp<width::W8>::encode;
        using detail::modrm_rsrc_with_disp<width::W16>::encode;
        using detail::modrm_rsrc_with_disp<width::W32>::encode;
        static detail::component_feedback<1uz> encode(bytes& b,std::byte mod,std::byte rm,std::byte rmreg){
            b.appendl((mod<<6uz) | (rmreg<<3uz) | rm);
            return {};
        }
    };
    namespace detail{
        // not type_identity_t! we use type_identity so we are a struct with a ::type member instead of a typedef
        template<typename T,typename U>
        using tpair = std::type_identity<pack<T,U>>;
        template<typename Comp,typename Rest,typename Packs>
        struct encode_comp_sig{};
        template<typename Comp,typename Rest,typename ...Argv,typename ...RestArgv>
        struct encode_comp_sig<Comp,Rest,pack<pack<Argv...>,pack<RestArgv...>>>{
            static auto encode(bytes& b,Argv ...a,RestArgv ...r)
            -> encode_feedback_prepend_t<Comp,decltype(Comp::encode(b,static_cast<Argv>(a)...)),decltype(Rest::encode(b,static_cast<RestArgv>(r)...))>{
                Comp::encode(b,static_cast<Argv>(a)...);
                Rest::encode(b,static_cast<RestArgv>(r)...);
                return {};
            }
        };
        template<typename Comp,typename Rest,typename Combop>
        struct encode_comp{};
        template<typename Comp,typename Rest,typename ...Combos>
        struct encode_comp<Comp,Rest,pack<Combos...>> : encode_comp_sig<Comp,Rest,Combos>...{
            using encode_comp_sig<Comp,Rest,Combos>::encode...;
        };
    }
    
    template<typename ...C>
    struct InstructionEncoding{
        using overloads = detail::pack<detail::pack<>>;
        static encode_feedback<> encode(bytes&){ return {}; }
    };
    template<typename Comp,typename ...Rest>
    struct InstructionEncoding<Comp,Rest...> : detail::encode_comp<Comp,InstructionEncoding<Rest...>,detail::outer_product_t<typename Comp::overloads,typename InstructionEncoding<Rest...>::overloads,detail::tpair>>{
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
        template<width w>
        using for_width = detail::select_by_width_t<w,Ie8,Ie16,Ie32,Ie64>;
    };
    template<template<width> typename Tp>
    struct rie_variable_width{};
    template<std::byte ...op>
    struct opcode_substitution{};
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
        template<typename T,typename S>
        struct subst_opcode{
            using type = T;
        };
        template<std::byte rexb,std::byte ...op,std::byte ...newop>
        struct subst_opcode<opcode<rexb,op...>,opcode_substitution<newop...>>{
            using type = opcode<rexb,newop...>;
        };
        template<typename T,typename S>
        using subst_opcode_t = subst_opcode<T,S>::type;
        
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
        
        template<typename Subst,typename ...C>
        struct do_reify_for_8bit{
            using type = InstructionEncoding<prefix<0x66_b>,subst_opcode_t<rie_reify_width_t<C,width::W8>,Subst>...>;
        };
        template<typename ...C>
        struct do_reify_for_8bit<void,C...>{
            using type = void;
        };
        template<typename Subst,typename ...C>
        using reify_for_8bit = do_reify_for_8bit<Subst,C...>::type;
        template<typename ...C>
        using reify_for_16bit = InstructionEncoding<prefix<0x66_b>,rie_reify_width_t<C,width::W16>...>;
        template<typename ...C>
        using reify_for_32bit = InstructionEncoding<rie_reify_width_t<C,width::W32>...>;
        template<typename ...C>
        using reify_for_64bit = InstructionEncoding<insert_rex_prefix_t<rex::W,rie_reify_width_t<C,width::W64>>...>;
    }
    template<typename Ie8s,typename ...C>
    using RegularInstructionEncodings = InstructionEncodings<detail::reify_for_8bit<Ie8s,C...>,detail::reify_for_16bit<C...>,detail::reify_for_32bit<C...>,detail::reify_for_64bit<C...>>;
    
    // only 8, 16 & 32 bit versions, like jumps
    template<typename Ie8s,typename ...C>
    using RegularShortInstructionEncodings = InstructionEncodings<detail::reify_for_8bit<Ie8s,C...>,detail::reify_for_16bit<C...>,detail::reify_for_32bit<C...>,void>;
    // only 32 & 64 bit versions, like movzx from 16-bit (or movzw* in AT&T syntax)
    template<typename ...C>
    using RegularLongInstructionEncodings = InstructionEncodings<void,void,detail::reify_for_32bit<C...>,detail::reify_for_64bit<C...>>;
    namespace instructions{
        namespace detail{
            constexpr std::byte addb(std::byte u,std::byte v){
                return static_cast<std::byte>(static_cast<std::uint8_t>(u)+static_cast<std::uint8_t>(v));
            }
            template<std::byte base>
            struct ins_rmr{
                using rm_r = RegularInstructionEncodings<
                    opcode_substitution<base>,
                    opcode_nr<addb(base,1_b)>,modrm<MODRM_R_SRC>
                >;
            };
            template<std::byte base>
            struct ins : ins_rmr<base>{
                using r_rm = RegularInstructionEncodings<
                    opcode_substitution<addb(base,2_b)>,
                    opcode_nr<addb(base,3_b)>,modrm<MODRM_R_DST>
                >;
            };
            template<std::byte base,std::byte opr>
            struct ins_rmi{
                using rm_imm = RegularInstructionEncodings<
                    opcode_substitution<base>,
                    opcode_nr<addb(base,1_b)>,modrm<opr>,rie_variable_width<immediate>
                >;
            };
        }
        struct add : detail::ins<0x00_b>, detail::ins_rmi<0x80_b,0_b>{};
        struct sub : detail::ins<0x28_b>, detail::ins_rmi<0x80_b,5_b>{};
        struct cmp : detail::ins<0x38_b>, detail::ins_rmi<0x80_b,7_b>{};
        struct mov : detail::ins<0x88_b>, detail::ins_rmi<0xC6_b,0_b>{};
        
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
        struct test : detail::ins_rmr<0x84_b>, detail::ins_rmi<0xF6_b,0_b>{};
        struct j{
            private:
                template<std::byte base>
                using generic = RegularShortInstructionEncodings<opcode_substitution<base>,opcode_nr<0x0F_b,detail::addb(base,0x10_b)>,rie_variable_width<signed_immediate>>;
            public:
                using z = generic<0x74_b>;
                using e = z;
                using le = generic<0x7E_b>;
        };
        struct jmp{
            using rel = RegularShortInstructionEncodings<opcode_substitution<0xEB_b>,opcode_nr<0xE9_b>,rie_variable_width<signed_immediate>>;
        };
    }
}
