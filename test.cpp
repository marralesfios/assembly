#include<iostream>
#include"instruction.hpp"
using namespace cppp::literals;
int main(){
    cppp::bytes buf;
    using namespace x86;
    std::size_t offs = instructions::add::rm_imm::for_width<width::W32>::encode(buf, 0x01_b /* [...]+disp8 */,reg::BP, displacement<width::W8>{4}, skip_immediate).offset_of_first<ComponentType::IMMEDIATE>;
    
    cppp::write<wv<width::W32>>(buf.data()+offs,13);
    std::cout.fill('0');
    std::cout.setf(std::ios_base::hex,std::ios_base::basefield);
    for(std::byte b : buf){
        std::cout.width(2);
        std::cout << static_cast<std::uint32_t>(b) << ' ';
    }
    std::cout.flush();
    return 0;
}
