//
//  ContentView.swift
//  lymos-proxy
//
//  Created by lymos on 2021/12/12.
//

import SwiftUI

struct ContentView: View {
    @State private var pac = ""
    @State private var status = ""
    @State private var btn_txt = "Start"
    var body: some View {
        HStack{
            Text("pac file: ")
            TextField("pac", text: $pac)
                .textFieldStyle(RoundedBorderTextFieldStyle())
        }.padding()
        HStack{
            Button(action: {
                if status == "started"{
                    status = "stoped"
                    btn_txt = "Start"
                }else{
                    status = "started"
                    btn_txt = "Stop"
                }
            }){
                Text(btn_txt)
            }.padding()
            TextField("ready", text: $status)
        }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
