import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';

import { SocketService } from './providers/socket.service';

import { AppComponent } from './app.component';
import { HeaderComponent } from './components/header/header.component';

@NgModule({
  declarations: [
    AppComponent,
    HeaderComponent
  ],
  imports: [
    BrowserModule
  ],
  providers: [SocketService],
  bootstrap: [AppComponent]
})
export class AppModule { }
