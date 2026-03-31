#include<iostream>
#include"instruction.hpp"
using namespace cppp::literals;
int main(){
    cppp::bytes buf;
    // x86::InstructionEncoding<x86::prefix<0x23_b>,x86::opcode_nr<0x21_b,0x06_b>,x86::modrm<x86::MODRM_R_DST>,x86::immediate<x86::width::W8>>::encode(buf,x86::reg::A,0x11_b,x86::reg::A,x86::displacement<x86::width::W8>{5},5);
    using namespace x86;
    std::size_t offs = InstructionEncoding<prefix<0x66_b>, opcode_nr<0x81_b>, modrm<0_b>, immediate<width::WORD>>::encode(buf, 0x01_b /* [...]+disp8 */ , 0_b /* A register */, displacement<width::W8>{5}, skip_immediate).offset_of_first<x86::ComponentType::IMMEDIATE>;
    
    cppp::write<x86::wv<x86::width::WORD>>(buf.data()+offs,13);
    std::cout.fill('0');
    std::cout.setf(std::ios_base::hex,std::ios_base::basefield);
    for(std::byte b : buf){
        std::cout.width(2);
        std::cout << static_cast<std::uint32_t>(b) << ' ';
    }
    std::cout.flush();
    return 0;
}
