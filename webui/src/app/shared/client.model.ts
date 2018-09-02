export class Client {
  hosts: string[] = [];

  constructor(
    public id: string
  ) { }

  setHosts(hosts: string[]) {
    this.hosts = hosts || [];
  }

  getHosts(): string {
    return this.hosts.join(', ') || 'N/A';
  }
}
