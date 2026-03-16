#include<iostream>
#include"instruction.hpp"
using namespace cppp::literals;
int main(){
    cppp::bytes buf;
    x86::InstructionEncoding<x86::prefix<0x23_b>,x86::opcode_nr<0x21_b,0x06_b>,x86::modrm<x86::MODRM_R_DST>,x86::immediate<x86::width::W8>>::encode(buf,x86::reg::A,0x11_b,x86::reg::A,x86::displacement<x86::width::W8>{5},5);
    std::cout.fill('0');
    std::cout.setf(std::ios_base::hex,std::ios_base::basefield);
    for(std::byte b : buf){
        std::cout.width(2);
        std::cout << static_cast<std::uint32_t>(b) << ' ';
    }
    std::cout.flush();
    return 0;
}
